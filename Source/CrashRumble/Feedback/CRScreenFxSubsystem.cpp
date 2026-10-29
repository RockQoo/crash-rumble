#include "Feedback/CRScreenFxSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/App.h"

namespace
{
	const FName ImpactParam(TEXT("Impact"));
	const FName FocusParam(TEXT("Focus"));
	const FName SeedParam(TEXT("Seed"));
	const FName FreezeParam(TEXT("Freeze"));
	const FName LightDirParam(TEXT("LightDir"));

	const FLinearColor FloorColor(0.6f, 0.62f, 0.68f);
	constexpr float FocusPatternInterval = 1.f / 12.f; // 초당 12번 패턴 재추첨
}

UCRScreenFxSubsystem* UCRScreenFxSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRScreenFxSubsystem>() : nullptr;
}

bool UCRScreenFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCRScreenFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRScreenFxSubsystem, STATGROUP_Tickables);
}

void UCRScreenFxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AnimeVolume = InWorld.SpawnActor<APostProcessVolume>(Params);
	FxVolume = InWorld.SpawnActor<APostProcessVolume>(Params);
	for (APostProcessVolume* Volume : { AnimeVolume.Get(), FxVolume.Get() })
	{
		if (Volume)
		{
			Volume->bUnbound = true;
			Volume->Priority = 10.f;
		}
	}

	if (AnimeVolume)
	{
		FPostProcessSettings& Settings = AnimeVolume->Settings;
		// 자동 노출이 켜져 있으면 Unlit 색이 계속 바뀐다
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
		Settings.bOverride_AutoExposureBias = true;
		// 비네트가 켜져 있으면 화면 가운데가 밝은 원처럼 보인다
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.f;
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.f;
		Settings.bOverride_BloomIntensity = true;
		Settings.BloomIntensity = 1.f;
	}

	if (UMaterialInterface* ScreenMaterial = GetDefault<UCRGameSettings>()->ScreenFxMaterial.LoadSynchronous())
	{
		FxMaterial = UMaterialInstanceDynamic::Create(ScreenMaterial, this);
		if (FxVolume)
		{
			FxVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, FxMaterial));
		}
	}

	SetupLightDirection();
	FindFloors();
	ApplyTuning();
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		TuningChangedHandle = Tuning->OnChanged.AddUObject(this, &UCRScreenFxSubsystem::ApplyTuning);
	}
}

void UCRScreenFxSubsystem::Deinitialize()
{
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		Tuning->OnChanged.Remove(TuningChangedHandle);
	}
	Super::Deinitialize();
}

void UCRScreenFxSubsystem::SetupLightDirection()
{
	UMaterialParameterCollection* Collection = GetDefault<UCRGameSettings>()->ToonParameters.LoadSynchronous();
	UMaterialParameterCollectionInstance* Instance = Collection ? GetWorld()->GetParameterCollectionInstance(Collection) : nullptr;
	if (!Instance)
	{
		return;
	}
	// Unlit이라 실제 조명은 무시되므로 레벨의 Directional Light 방향을 복사해 명암만 계산한다
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		const FVector ToLight = -It->GetComponent()->GetForwardVector();
		Instance->SetVectorParameterValue(LightDirParam, FLinearColor(ToLight.X, ToLight.Y, ToLight.Z, 0.f));
		break;
	}
}

void UCRScreenFxSubsystem::FindFloors()
{
	// 템플릿 바닥 (이름에 Floor). 툰으로 바꿀 때를 위해 원래 머티리얼을 기억해 둔다
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		if (It->GetName().Contains(TEXT("Floor")) || It->GetActorNameOrLabel().Contains(TEXT("Floor")))
		{
			if (UStaticMeshComponent* Mesh = It->GetStaticMeshComponent())
			{
				Floors.Add(Mesh);
				FloorOriginalMaterials.Add(Mesh->GetMaterial(0));
			}
		}
	}
}

void UCRScreenFxSubsystem::ApplyFloorMaterial()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const bool bToon = Tuning.bToonShading && Tuning.bToonFloor;
	if (bToon == bFloorToon)
	{
		return;
	}
	bFloorToon = bToon;
	UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial();
	for (int32 i = 0; i < Floors.Num(); ++i)
	{
		UStaticMeshComponent* Floor = Floors[i];
		if (!Floor)
		{
			continue;
		}
		if (bToon && Toon)
		{
			Floor->SetMaterial(0, Toon);
			Floor->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(FloorColor.R, FloorColor.G, FloorColor.B, 0.f));
			Floor->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, 0.f);
			Floor->SetCustomPrimitiveDataFloat(CRMaterialData::CheckerIndex, 1.f);
		}
		else
		{
			Floor->SetMaterial(0, FloorOriginalMaterials[i]);
		}
	}
}

void UCRScreenFxSubsystem::ApplyTuning()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (AnimeVolume)
	{
		AnimeVolume->bEnabled = Tuning.bAnimePostProcess;
		AnimeVolume->Settings.AutoExposureBias = Tuning.ExposureBias;
	}
	ApplyFloorMaterial();
}

void UCRScreenFxSubsystem::RequestImpactFrame(bool bForce)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (!Tuning.bImpactFrame || (!bForce && Now < ImpactCooldownEnd))
	{
		return;
	}
	ImpactStart = Now;
	ImpactCooldownEnd = Now + Tuning.ImpactFrameCooldown;
}

void UCRScreenFxSubsystem::AddFocusLines(float Amount, float Duration)
{
	if (Now >= FocusEnd)
	{
		FocusTimed = 0.f;
	}
	FocusTimed = FMath::Max(FocusTimed, Amount);
	FocusEnd = FMath::Max(FocusEnd, Now + Duration);
}

void UCRScreenFxSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_ScreenFxTick);
	Super::Tick(DeltaTime);

	const float RealDelta = FApp::GetDeltaTime();
	Now += RealDelta;
	if (!FxMaterial)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 임팩트 프레임: 2장 (① 주인공+아주 밝은 이펙트 = 검은 실루엣, 나머지 흰색 ② 반전)
	float Impact = 0.f;
	if (ImpactStart >= 0.0)
	{
		const double T = Now - ImpactStart;
		const float Frame = FMath::Max(Tuning.ImpactFrameDuration, 0.005f);
		Impact = T < Frame ? 1.f : T < Frame * 2.f ? 2.f : 0.f;
		if (Impact == 0.f)
		{
			ImpactStart = -1.0;
		}
	}

	float Focus = Now < FocusEnd ? FocusTimed : 0.f;
	Focus = FMath::Max(Focus, SustainedFocus);
	SustainedFocus = 0.f;
	if (!Tuning.bFocusLines)
	{
		Focus = 0.f;
	}

	SeedTimer += RealDelta;
	if (SeedTimer >= FocusPatternInterval)
	{
		SeedTimer = 0.f;
		Seed = FMath::FRandRange(0.f, 100.f);
	}

	FxMaterial->SetScalarParameterValue(ImpactParam, Impact);
	FxMaterial->SetScalarParameterValue(FocusParam, Focus);
	FxMaterial->SetScalarParameterValue(SeedParam, Seed);
	FxMaterial->SetScalarParameterValue(FreezeParam, FreezeAmount);
}
