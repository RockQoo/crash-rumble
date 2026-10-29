#include "Core/CRBenchmark.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Core/CRActorPool.h"
#include "Core/CRTargetSpawner.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "DynamicRHI.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Map/CRBlockArena.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "RenderTimer.h"
#include "Weapon/CRMissile.h"
#include "Weapon/CRProjectile.h"

namespace
{
	const TCHAR* BenchMapName = TEXT("bench");
	const TCHAR* TraceChannels = TEXT("default,cpu,gpu,frame,bookmark,counters");
	constexpr float ShoulderRestorePitch = -10.f;
	constexpr float MapEdgeMargin = 200.f;

	/** 지금 커밋 (.git/HEAD → ref 파일 또는 packed-refs). 못 찾으면 "unknown". */
	FString ReadGitCommit()
	{
		const FString GitDir = FPaths::Combine(FPaths::ProjectDir(), TEXT(".git"));
		FString Head;
		if (!FFileHelper::LoadFileToString(Head, *FPaths::Combine(GitDir, TEXT("HEAD"))))
		{
			return TEXT("unknown");
		}
		Head.TrimStartAndEndInline();
		if (!Head.StartsWith(TEXT("ref: ")))
		{
			return Head;
		}
		const FString Ref = Head.RightChop(5);
		FString Hash;
		if (FFileHelper::LoadFileToString(Hash, *FPaths::Combine(GitDir, Ref)))
		{
			return Hash.TrimStartAndEnd();
		}
		TArray<FString> Packed;
		if (FFileHelper::LoadFileToStringArray(Packed, *FPaths::Combine(GitDir, TEXT("packed-refs"))))
		{
			for (const FString& Line : Packed)
			{
				if (Line.EndsWith(Ref))
				{
					return Line.Left(40);
				}
			}
		}
		return TEXT("unknown");
	}
}

UCRBenchmarkSubsystem* UCRBenchmarkSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRBenchmarkSubsystem>() : nullptr;
}

TStatId UCRBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRBenchmarkSubsystem, STATGROUP_Tickables);
}

bool UCRBenchmarkSubsystem::Start(FString& OutMessage)
{
	UWorld* World = GetWorld();
	ACRCharacter* Player = Cast<ACRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this);
	if (!World || !Player || !Spawner)
	{
		OutMessage = TEXT("벤치마크는 캐릭터로 플레이할 때만 (타격 체크 / 인게임)");
		return false;
	}
	if (IsActive())
	{
		Stop();
	}
	bConditionsChanged = false;
	UnbindTuning();
	if (UCRTuningSubsystem* TuningSubsystem = UCRTuningSubsystem::Get(this))
	{
		TuningChangedHandle = TuningSubsystem->OnChanged.AddWeakLambda(this, [this]() { bConditionsChanged = true; });
	}

	// 원래 맵을 기억해 두고 큰 평지로 (지금 맵에서 재면 그대로, 중지할 때 다시 불러와 지형 복구)
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	ACRBlockArena* Arena = ACRBlockArena::Find(this);
	PreviousMap = Arena ? Arena->GetMapName() : FString();
	if (!Tuning.bBenchCurrentMap || !Arena)
	{
		Arena = ACRBlockArena::LoadMap(World, BenchMapName, OutMessage);
	}
	if (!Arena)
	{
		return false;
	}

	// 플레이어를 가운데로 → 전투는 플레이어 위치/시선 기준으로 좌(레드) 우(블루).
	// 관전 중인 플레이어는 숨겨지고 맞지 않으며, 카메라는 높이서 전장 전체를 내려다본다
	const FVector Center = Arena->GetCenterTop();
	Player->TeleportTo(Center + FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator);
	if (AController* Controller = Player->GetController())
	{
		Controller->SetControlRotation(FRotator(-Tuning.BenchCameraPitch, 0.f, 0.f));
	}
	if (UCRViewComponent* View = Player->GetViewComponent())
	{
		View->SetOverviewArmLength(Tuning.BenchCameraDistance);
	}

	FCRBattleSetup Setup;
	Setup.PerTeam = Tuning.BenchPerTeam;
	Setup.SwordWeight = 0;
	Setup.LaserWeight = Tuning.BenchLaserRatio;
	Setup.ProjectileWeight = Tuning.BenchProjectileRatio;
	Setup.MissileWeight = Tuning.BenchMissileRatio;
	Setup.TeamDistance = Tuning.BenchTeamDistance;
	Setup.Spread = Tuning.BenchSpread;
	Setup.Strafe = Tuning.BenchStrafe;
	Setup.bSpectate = true;
	Setup.bLineFormation = true;
	Setup.FrontWidth = Tuning.BenchFrontWidth;
	Setup.PerRow = Tuning.BenchPerRow;
	Setup.RowSpacing = Tuning.BenchRowSpacing;
	Setup.EngageRange = Tuning.BenchEngageRange;
	Setup.DamageScale = Tuning.BenchDamageScale;
	// 맵이 작으면 (지금 맵에서 잴 때) 진형을 맵 안으로 줄인다: 진영은 ±Y, 전열은 X 방향
	const FVector2D Half = Arena->GetHalfSize() - FVector2D(MapEdgeMargin);
	const int32 Rows = FMath::DivideAndRoundUp(FMath::Max(Setup.PerTeam, 1), FMath::Max(Setup.PerRow, 1));
	Setup.TeamDistance = FMath::Clamp(Setup.TeamDistance, 100.f, FMath::Max(Half.Y, 100.f));
	Setup.RowSpacing = FMath::Min(Setup.RowSpacing, Setup.TeamDistance / FMath::Max(Rows, 1));
	Setup.FrontWidth = FMath::Min(Setup.FrontWidth, FMath::Max(Half.X * 2.f, 100.f));
	Spawner->StartBattle(Setup);

	PerTeam = Setup.PerTeam;
	MapName = Arena->GetMapName();
	RunWarmup = Tuning.BenchWarmup;
	RunDuration = Tuning.BenchDuration;
	bRunTrace = Tuning.bBenchTrace;
	bRunLegacyBlocks = Tuning.bBenchLegacyBlocks;
	RunConditions = FString::Printf(
		TEXT("perTeam=%d weights(sword/laser/projectile/missile)=%d/%d/%d/%d teamDistance=%.0f frontWidth=%.0f perRow=%d rowSpacing=%.0f ")
		TEXT("engageRange=%.0f damageScale=%.2f strafe=%.2f craterRadius=%.0f gunCraterRadius=%.0f charLod=%d legacyBlocks=%d"),
		Setup.PerTeam, Setup.SwordWeight, Setup.LaserWeight, Setup.ProjectileWeight, Setup.MissileWeight, Setup.TeamDistance, Setup.FrontWidth,
		Setup.PerRow, Setup.RowSpacing, Setup.EngageRange, Setup.DamageScale, Setup.Strafe, Tuning.BenchCraterRadius, Tuning.BenchGunCraterRadius,
		Tuning.bCharLod ? 1 : 0, bRunLegacyBlocks ? 1 : 0);
	Samples.Reset();
	ResultText.Reset();
	Phase = EPhase::Warmup;
	PhaseStartSeconds = FPlatformTime::Seconds();
	OutMessage = FString::Printf(TEXT("벤치마크 시작: %d vs %d"), PerTeam, PerTeam);
	return true;
}

void UCRBenchmarkSubsystem::Stop()
{
	if (Phase == EPhase::Idle)
	{
		return;
	}
	if (Phase == EPhase::Measure)
	{
		FinishMeasure();
	}
	Phase = EPhase::Idle;
	UnbindTuning();

	if (UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
	{
		Spawner->ClearTargets();
	}
	if (ACRCharacter* Player = Cast<ACRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UCRViewComponent* View = Player->GetViewComponent())
		{
			View->SetOverviewArmLength(0.f);
		}
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(FRotator(ShoulderRestorePitch, 0.f, 0.f));
		}
	}
	UWorld* World = GetWorld();
	FString Message;
	if (PreviousMap.IsEmpty())
	{
		ACRBlockArena::ClearMap(World);
	}
	else if (const ACRBlockArena* Arena = ACRBlockArena::LoadMap(World, PreviousMap, Message))
	{
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Player->TeleportTo(Arena->GetCenterTop() + FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator);
		}
	}
}

void UCRBenchmarkSubsystem::Deinitialize()
{
	if (bTracing)
	{
		FTraceAuxiliary::Stop();
		bTracing = false;
	}
	UnbindTuning();
	Super::Deinitialize();
}

void UCRBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Phase == EPhase::Idle || Phase == EPhase::Done)
	{
		return;
	}
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartSeconds;
	if (Phase == EPhase::Warmup)
	{
		if (Elapsed >= RunWarmup)
		{
			BeginMeasure();
		}
		return;
	}

	// 실제 시간 기준 (히트스톱/슬로모션과 무관)
	Samples.Add(TakeSample());
	if (Elapsed >= RunDuration)
	{
		FinishMeasure();
	}
}

UCRBenchmarkSubsystem::FSample UCRBenchmarkSubsystem::TakeSample() const
{
	FSample Sample;
	Sample.FrameMs = float(FApp::GetDeltaTime() * 1000.0);
	Sample.GameMs = float(FPlatformTime::ToMilliseconds(GGameThreadTime));
	Sample.RenderMs = float(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	Sample.GpuMs = float(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
	if (const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
	{
		Sample.Units = Spawner->GetAliveCount(ECRTeam::Red) + Spawner->GetAliveCount(ECRTeam::Blue);
	}
	if (const UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this))
	{
		Sample.Missiles = Pool->GetActiveCount(ACRMissile::StaticClass());
		Sample.Projectiles = Pool->GetActiveCount(ACRProjectile::StaticClass());
	}
	if (const ACRBlockArena* Arena = ACRBlockArena::Find(this))
	{
		Sample.Blocks = Arena->GetBlockCount();
		Sample.Falling = Arena->GetFallingCount();
	}
	return Sample;
}

void UCRBenchmarkSubsystem::BeginMeasure()
{
	Phase = EPhase::Measure;
	PhaseStartSeconds = FPlatformTime::Seconds();
	Samples.Reset();
	FileBase = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"), TEXT("CRBench"),
		FString::Printf(TEXT("Bench_%s_%dvs%d_%s"), *MapName, PerTeam, PerTeam, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")))));

	SpawnedAtMeasure = TakeSample().Units;
	bTracing = false;
	if (bRunTrace)
	{
		// 에디터가 트레이스 서버로 이미 보내고 있으면 끊고 파일로 다시 시작
		if (FTraceAuxiliary::IsConnected())
		{
			FTraceAuxiliary::Stop();
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FileBase), true);
		bTracing = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *(FileBase + TEXT(".utrace")), TraceChannels);
		TRACE_BOOKMARK(TEXT("CRBench Measure Start"));
	}
}

void UCRBenchmarkSubsystem::FinishMeasure()
{
	Phase = EPhase::Done;
	if (bTracing)
	{
		TRACE_BOOKMARK(TEXT("CRBench Measure End"));
		FTraceAuxiliary::Stop();
	}
	if (Samples.Num() == 0)
	{
		ResultText = TEXT("측정된 프레임이 없음");
		return;
	}

	// 요약: 평균, 1% 최저(느린 쪽 1% 프레임의 평균), 최악 프레임, 스레드별 평균
	double SumFrame = 0.0, SumGame = 0.0, SumRender = 0.0, SumGpu = 0.0, SumUnits = 0.0;
	int32 MaxMissiles = 0, MaxProjectiles = 0, MinBlocks = TNumericLimits<int32>::Max(), MaxBlocks = 0, MaxFalling = 0;
	TArray<float> FrameTimes;
	FString Csv = TEXT("Frame,FrameMs,GameMs,RenderMs,GpuMs,Units,Missiles,Projectiles,Blocks,Falling\n");
	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		const FSample& S = Samples[i];
		SumFrame += S.FrameMs;
		SumGame += S.GameMs;
		SumRender += S.RenderMs;
		SumGpu += S.GpuMs;
		SumUnits += S.Units;
		MaxMissiles = FMath::Max(MaxMissiles, S.Missiles);
		MaxProjectiles = FMath::Max(MaxProjectiles, S.Projectiles);
		MinBlocks = FMath::Min(MinBlocks, S.Blocks);
		MaxBlocks = FMath::Max(MaxBlocks, S.Blocks);
		MaxFalling = FMath::Max(MaxFalling, S.Falling);
		FrameTimes.Add(S.FrameMs);
		Csv += FString::Printf(TEXT("%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d\n"), i, S.FrameMs, S.GameMs, S.RenderMs, S.GpuMs,
			S.Units, S.Missiles, S.Projectiles, S.Blocks, S.Falling);
	}
	const int32 Count = Samples.Num();
	FrameTimes.Sort([](float A, float B) { return A > B; });
	const int32 WorstCount = FMath::Max(1, Count / 100);
	double WorstSum = 0.0;
	for (int32 i = 0; i < WorstCount; ++i)
	{
		WorstSum += FrameTimes[i];
	}
	const float AvgFrame = float(SumFrame / Count);
	const float AvgGame = float(SumGame / Count);
	const float AvgRender = float(SumRender / Count);
	const float AvgGpu = float(SumGpu / Count);
	const float Low1Ms = float(WorstSum / WorstCount);
	const TCHAR* Bottleneck = AvgGame >= AvgRender && AvgGame >= AvgGpu ? TEXT("게임 스레드")
		: AvgRender >= AvgGpu ? TEXT("렌더 스레드") : TEXT("GPU");

	ResultText = FString::Printf(
		TEXT("벤치마크 결과: %s 맵, %d vs %d, %.0f초, %d프레임%s\n")
		TEXT("평균 FPS %.1f (%.1f ms) · 1%% 최저 FPS %.1f · 최악 %.1f ms\n")
		TEXT("게임 %.1f ms · 렌더 %.1f ms · GPU %.1f ms → 병목: %s\n")
		TEXT("유닛 평균 %.0f · 미사일 최대 %d · 총알 최대 %d · 블럭 최대 %d개 파임 · 낙하 최대 %d\n")
		TEXT("%s.csv%s"),
		*MapName, PerTeam, PerTeam, SumFrame / 1000.0, Count,
		bRunLegacyBlocks ? TEXT(" [블럭 옛 방식]") : TEXT(""),
		1000.f / FMath::Max(AvgFrame, 0.001f), AvgFrame, 1000.f / FMath::Max(Low1Ms, 0.001f), FrameTimes[0],
		AvgGame, AvgRender, AvgGpu, Bottleneck,
		SumUnits / Count, MaxMissiles, MaxProjectiles, MaxBlocks - MinBlocks, MaxFalling,
		*FPaths::GetCleanFilename(FileBase), bTracing ? TEXT(" + .utrace") : TEXT(""));
	if (bConditionsChanged)
	{
		ResultText += TEXT("\n주의: 시작 후 튜닝 값이 바뀌었음 → 다른 결과와 비교하지 말 것");
	}
	bTracing = false;
	WriteMetadata();

	FFileHelper::SaveStringToFile(Csv, *(FileBase + TEXT(".csv")), FFileHelper::EEncodingOptions::ForceUTF8);
	FFileHelper::SaveStringToFile(ResultText, *(FileBase + TEXT(".txt")), FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogTemp, Display, TEXT("CRBench\n%s\n(%s)"), *ResultText, *FileBase);
}

void UCRBenchmarkSubsystem::UnbindTuning()
{
	if (TuningChangedHandle.IsValid())
	{
		if (UCRTuningSubsystem* TuningSubsystem = UCRTuningSubsystem::Get(this))
		{
			TuningSubsystem->OnChanged.Remove(TuningChangedHandle);
		}
		TuningChangedHandle.Reset();
	}
}

void UCRBenchmarkSubsystem::WriteMetadata() const
{
	// 결과만으로 재현할 수 있게: 커밋, 맵 파일 해시, 유효 진형, 실제 생성 수, 환경, 전체 튜닝 값
	FString MapHash = TEXT("unknown");
	TArray<uint8> MapBytes;
	if (FFileHelper::LoadFileToArray(MapBytes, *CRMapFile::ResolvePath(MapName)))
	{
		MapHash = FMD5::HashBytes(MapBytes.GetData(), MapBytes.Num());
	}
	FIntPoint Resolution = FIntPoint::ZeroValue;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		Resolution = GEngine->GameViewport->Viewport->GetSizeXY();
	}

	FString Meta;
	Meta += FString::Printf(TEXT("commit=%s\n"), *ReadGitCommit());
	Meta += FString::Printf(TEXT("map=%s md5=%s\n"), *MapName, *MapHash);
	Meta += FString::Printf(TEXT("conditions=%s\n"), *RunConditions);
	Meta += FString::Printf(TEXT("warmup=%.1f duration=%.1f trace=%d spawnedAtMeasure=%d conditionsChanged=%d\n"),
		RunWarmup, RunDuration, bRunTrace ? 1 : 0, SpawnedAtMeasure, bConditionsChanged ? 1 : 0);
	Meta += TEXT("seed=none (전투·재생 난수는 고정하지 않음 → 같은 조건으로 3회 이상 재서 비교)\n");
	Meta += FString::Printf(TEXT("build=%s editor=%d resolution=%dx%d\n"), LexToString(FApp::GetBuildConfiguration()), GIsEditor ? 1 : 0,
		Resolution.X, Resolution.Y);
	Meta += FString::Printf(TEXT("cpu=%s\ngpu=%s\n"), *FPlatformMisc::GetCPUBrand().TrimStartAndEnd(), *FPlatformMisc::GetPrimaryGPUBrand());
	Meta += TEXT("[tuning]\n");
	if (const UCRTuningSubsystem* TuningSubsystem = UCRTuningSubsystem::Get(this))
	{
		for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
		{
			Meta += FString::Printf(TEXT("%s=%g\n"), *Item.GetName().ToString(), Item.GetValue(TuningSubsystem->GetMutableSettings()));
		}
	}
	FFileHelper::SaveStringToFile(Meta, *(FileBase + TEXT(".meta.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
}

FText UCRBenchmarkSubsystem::GetStatusText() const
{
	const double Elapsed = FPlatformTime::Seconds() - PhaseStartSeconds;
	switch (Phase)
	{
	case EPhase::Warmup:
		return FText::FromString(FString::Printf(TEXT("벤치마크 준비 중 %.0f / %.0f초"), Elapsed, RunWarmup));
	case EPhase::Measure:
		return FText::FromString(FString::Printf(TEXT("벤치마크 측정 중 %.0f / %.0f초%s%s"), Elapsed, RunDuration,
			bTracing ? TEXT(" · 트레이스 기록 중") : TEXT(""), bConditionsChanged ? TEXT(" · 튜닝 바뀜(비교 제외)") : TEXT("")));
	case EPhase::Done:
		return FText::FromString(ResultText + TEXT("\n(전투 계속 중 · 중지하면 원래 맵으로)"));
	default:
		return FText::FromString(ResultText);
	}
}
