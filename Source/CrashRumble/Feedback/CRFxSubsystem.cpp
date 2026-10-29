#include "Feedback/CRFxSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

namespace
{
	constexpr int32 MaxPieces = 4096;
	constexpr int32 MaxProcs = 256;
	const FTransform HiddenTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);

	// 구체 폭발 단계 색 (그라데이션 없이 끊는다)
	const FLinearColor FireGlow(3.f, 2.6f, 1.2f);
	const FLinearColor FireOrange(1.f, 0.45f, 0.08f);
	const FLinearColor FireRed(0.75f, 0.1f, 0.05f);
	const FLinearColor FireSmoke(0.2f, 0.16f, 0.18f);

	// 연기
	const FLinearColor SmokeGlow(2.5f, 2.5f, 2.5f);
	const FLinearColor SmokeWhite(0.95f, 0.95f, 1.f);
	const FLinearColor SmokeGray(0.55f, 0.55f, 0.6f);
	const FLinearColor DustColor(0.85f, 0.78f, 0.66f);

	// 번개
	const FLinearColor LightningColor(1.5f, 3.f, 6.f);

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Square(1.f - T);
	}

	FVector AnyPerpendicular(const FVector& Dir)
	{
		const FVector Up = FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		return FVector::CrossProduct(Dir, Up).GetSafeNormal();
	}
}

UCRFxSubsystem* UCRFxSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRFxSubsystem>() : nullptr;
}

bool UCRFxSubsystem::IsFxDrawFrame(const UObject* WorldContext)
{
	const UCRFxSubsystem* Fx = Get(WorldContext);
	return !Fx || Fx->IsFxDrawFrame();
}

bool UCRFxSubsystem::IsCharSteppingEnabled() const
{
	return UCRTuningSubsystem::GetSettings(this).CharFps > 0;
}

bool UCRFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCRFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRFxSubsystem, STATGROUP_Tickables);
}

void UCRFxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Pieces.SetNum(MaxPieces);
	Procs.SetNum(MaxProcs);
	ShapeLists.SetNum(static_cast<int32>(ECRFxShape::Count));
	TickStartHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UCRFxSubsystem::HandleWorldTickStart);
}

void UCRFxSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
	Super::Deinitialize();
}

void UCRFxSubsystem::HandleWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld())
	{
		return;
	}
	// 그리기 판단은 프레임 시작에 한 번: 캐릭터/이펙트가 같은 프레임에 같은 결정을 본다.
	// 월드 시간 기준이라 히트스톱·시간 정지 중에는 그림도 멈춘다 (단, 실시간 이펙트가 있으면 실시간으로 진행)
	const AWorldSettings* WorldSettings = World->GetWorldSettings();
	const float Dilation = WorldSettings ? WorldSettings->GetEffectiveTimeDilation() : 1.f;
	const float FxDelta = HasRealTimeEffects() ? FMath::Max(DeltaSeconds, DeltaSeconds * Dilation) : DeltaSeconds * Dilation;
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	auto Step = [](float& Accumulator, float Delta, int32 Fps)
	{
		if (Fps <= 0)
		{
			Accumulator = 0.f;
			return true;
		}
		const float Interval = 1.f / Fps;
		Accumulator += Delta;
		if (Accumulator >= Interval)
		{
			Accumulator = FMath::Fmod(Accumulator, Interval);
			return true;
		}
		return false;
	};
	bFxDrawFrame = Step(FxAccumulator, FxDelta, Tuning.FxFps);
	bCharDrawFrame = Step(CharAccumulator, DeltaSeconds * Dilation, Tuning.CharFps);
	if (bFxDrawFrame)
	{
		++FxDrawCount;
	}
}

bool UCRFxSubsystem::HasRealTimeEffects() const
{
	if (AliveProcs == 0)
	{
		return false;
	}
	for (const FProc& Proc : Procs)
	{
		if (Proc.bAlive && Proc.bRealTime)
		{
			return true;
		}
	}
	return false;
}

UInstancedStaticMeshComponent* UCRFxSubsystem::MakeComponent(UStaticMesh* Mesh, UMaterialInterface* Material, int32 NumCustomFloats)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(FxActor);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetNumCustomDataFloats(NumCustomFloats);
	Component->SetupAttachment(FxActor->GetRootComponent());
	Component->RegisterComponent();
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	return Component;
}

bool UCRFxSubsystem::EnsureComponents()
{
	if (FxActor)
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	FxActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	USceneComponent* Root = NewObject<USceneComponent>(FxActor, TEXT("Root"));
	FxActor->SetRootComponent(Root);
	Root->RegisterComponent();

	UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial();
	ShapeComponents.SetNum(static_cast<int32>(ECRFxShape::Count));
	for (int32 i = 0; i < ShapeComponents.Num(); ++i)
	{
		ShapeComponents[i] = MakeComponent(CRShapeMeshes::Get(static_cast<ECRFxShape>(i)), Toon, CRMaterialData::InstanceFloats);
	}

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	BlobComponent = MakeComponent(Plane, GetDefault<UCRGameSettings>()->BlobShadowMaterial.LoadSynchronous(), 1);
	BlobComponent->SetTranslucentSortPriority(-1);
	return true;
}

UCRFxSubsystem::FPiece& UCRFxSubsystem::NextPiece()
{
	// 고정 버퍼를 돌려 쓴다: 꽉 차면 가장 오래된 것을 덮어씀
	FPiece& Piece = Pieces[NextPieceIndex];
	NextPieceIndex = (NextPieceIndex + 1) % MaxPieces;
	if (!Piece.bAlive)
	{
		++AlivePieces;
	}
	Piece = FPiece();
	Piece.bAlive = true;
	return Piece;
}

UCRFxSubsystem::FProc& UCRFxSubsystem::NextProc()
{
	FProc& Proc = Procs[NextProcIndex];
	NextProcIndex = (NextProcIndex + 1) % MaxProcs;
	if (!Proc.bAlive)
	{
		++AliveProcs;
	}
	Proc = FProc();
	Proc.bAlive = true;
	Proc.Seed = SeedCounter++;
	return Proc;
}

void UCRFxSubsystem::AddBlobShadow(const FVector& GroundPoint, float Radius, float Strength)
{
	if (Strength <= 0.f || Radius <= 0.f)
	{
		return;
	}
	BlobList.Transforms.Add(FTransform(FQuat::Identity, GroundPoint + FVector(0.f, 0.f, 1.5f), FVector(Radius / 50.f, Radius / 50.f, 1.f)));
	BlobList.Custom.Add(Strength);
}

void UCRFxSubsystem::SpawnDebris(const FVector& Location, const FVector& Normal, int32 Count, const FLinearColor& ColorA,
	const FLinearColor& ColorB, float SizeScale, float GroundZ)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	Count = FMath::RoundToInt(Count * Tuning.DebrisCountScale);
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	const FVector Side = AnyPerpendicular(N);
	for (int32 i = 0; i < Count; ++i)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::Debris;
		P.Shape = ECRFxShape::Cube;
		// 작은 것 위주, 가끔 큰 것 (랜덤값 제곱)
		const float Size = FMath::Lerp(Tuning.DebrisMinSize, Tuning.DebrisMaxSize, FMath::Square(FMath::FRand())) * SizeScale * Tuning.DebrisSize;
		P.Size = Size / 100.f;
		// 공중에서 갑자기 생기지 않고 표면 안쪽(크기의 일정 비율)에서 튀어나온다
		P.Location = Location - N * Size * Tuning.DebrisBury;
		const FVector Sideways = Side.RotateAngleAxis(FMath::FRandRange(0.f, 360.f), N);
		P.Velocity = N * FMath::FRandRange(Tuning.DebrisNormalSpeedMin, Tuning.DebrisNormalSpeedMax)
			+ Sideways * FMath::FRandRange(Tuning.DebrisSideSpeedMin, Tuning.DebrisSideSpeedMax)
			+ FVector::UpVector * FMath::FRandRange(Tuning.DebrisUpSpeedMin, Tuning.DebrisUpSpeedMax);
		P.Rotation = FMath::VRand().ToOrientationQuat();
		P.SpinAxis = FMath::VRand();
		P.SpinSpeed = FMath::DegreesToRadians(FMath::FRandRange(Tuning.DebrisSpinMin, Tuning.DebrisSpinMax));
		P.Lifetime = FMath::FRandRange(Tuning.DebrisLifeMin, Tuning.DebrisLifeMax);
		P.GroundZ = GroundZ;
		P.Color = (i % 2 == 0) ? ColorA : ColorB;
	}
}

void UCRFxSubsystem::SpawnSphereExplosion(const FVector& Center, float Radius, float GroundZ)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	// 섬광: 딱 두 장 (흰 큰 구 → 노란 작은 구)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::Flash;
		P.Shape = ECRFxShape::Sphere;
		P.Location = Center;
		P.Size = Radius / 50.f;
		P.Lifetime = FMath::Max(Tuning.ExplosionFlashEnd, 0.001f);
		P.Flat = 1.f;
	}

	// 불덩이: 위쪽 반구에 퍼뜨린다 (바닥으로 파고들지 않게)
	for (int32 i = 0; i < Tuning.FireballCount; ++i)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::Fireball;
		P.Shape = ECRFxShape::Sphere;
		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z);
		P.Location = Center + Dir * Radius * FMath::FRandRange(Tuning.FireballSpreadMin, Tuning.FireballSpreadMax);
		if (GroundZ > -UE_BIG_NUMBER * 0.5f)
		{
			P.Location.Z = FMath::Max(P.Location.Z, GroundZ + Radius * 0.2f);
		}
		P.Size = Radius * FMath::FRandRange(Tuning.FireballSizeMin, Tuning.FireballSizeMax) / 50.f;
		P.Delay = FMath::FRandRange(0.f, Tuning.FireballDelayMax);
		P.Lifetime = FMath::FRandRange(Tuning.FireballLifeMin, Tuning.FireballLifeMax);
	}

	// 바닥 충격파: 작은 큐브가 원형으로 바닥을 따라 0.3배 → 2.2배까지 퍼지며 작아짐
	const float RingZ = GroundZ > -UE_BIG_NUMBER * 0.5f ? GroundZ + 6.f : Center.Z;
	const float Start = FMath::FRandRange(0.f, UE_TWO_PI);
	const int32 RingCount = FMath::Max(Tuning.ShockCubeCount, 0);
	for (int32 i = 0; i < RingCount; ++i)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::RingCube;
		P.Shape = ECRFxShape::Cube;
		const float Angle = Start + UE_TWO_PI * i / RingCount;
		P.Origin = FVector(Center.X, Center.Y, RingZ);
		P.Velocity = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
		P.Rotation = FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f).Quaternion();
		P.Size = FMath::Clamp(Radius * 0.12f, 4.f, 20.f) / 100.f;
		P.Lifetime = Tuning.ShockDuration;
		P.Color = DustColor;
	}
}

void UCRFxSubsystem::SpawnHitStar(const FVector& Location, float Size, const FLinearColor& Color)
{
	FProc& Proc = NextProc();
	Proc.Kind = EProcKind::HitStar;
	Proc.A = Location;
	Proc.Size = Size * UCRTuningSubsystem::GetSettings(this).HitStarSize;
	Proc.Color = Color;
	Proc.Lifetime = UCRTuningSubsystem::GetSettings(this).HitStarDuration;
}

void UCRFxSubsystem::SpawnSparkle(const FVector& Location, float Size, USceneComponent* Follow, bool bRealTime)
{
	FProc& Proc = NextProc();
	Proc.Kind = EProcKind::Sparkle;
	Proc.A = Location;
	Proc.Size = Size;
	Proc.Lifetime = FMath::Max(UCRTuningSubsystem::GetSettings(this).SparkleDuration, 0.02f);
	Proc.bRealTime = bRealTime;
	if (Follow)
	{
		Proc.Follow = Follow;
		Proc.FollowOffset = Follow->GetComponentTransform().InverseTransformPosition(Location);
	}
}

void UCRFxSubsystem::SpawnKanada(const FVector& Location, float Size)
{
	FProc& Proc = NextProc();
	Proc.Kind = EProcKind::Kanada;
	Proc.A = Location;
	Proc.Size = Size * UCRTuningSubsystem::GetSettings(this).KanadaSize;
	Proc.Lifetime = UCRTuningSubsystem::GetSettings(this).KanadaDuration;
}

void UCRFxSubsystem::SpawnDustRing(const FVector& GroundLocation, float Size)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::DustRing;
		P.Shape = ECRFxShape::Donut;
		P.Location = GroundLocation + FVector(0.f, 0.f, 4.f);
		P.Size = Size;
		P.Lifetime = Tuning.DustRingDuration;
		P.Color = DustColor;
	}
	const float Start = FMath::FRandRange(0.f, UE_TWO_PI);
	const int32 PuffCount = FMath::Max(Tuning.DustPuffCount, 0);
	for (int32 i = 0; i < PuffCount; ++i)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::DustPuff;
		P.Shape = ECRFxShape::Sphere;
		const float Angle = Start + UE_TWO_PI * i / PuffCount;
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		P.Location = GroundLocation + Dir * 30.f * Size + FVector(0.f, 0.f, 10.f * Size);
		P.Velocity = Dir * Tuning.DustPuffSpeed * Size + FVector(0.f, 0.f, 40.f);
		P.Size = Tuning.DustPuffSize * Size * FMath::FRandRange(0.7f, 1.2f);
		P.Lifetime = Tuning.DustRingDuration;
		P.Color = DustColor;
	}
}

void UCRFxSubsystem::SpawnLightning(const FVector& Start, const FVector& End, float Duration)
{
	FProc& Proc = NextProc();
	Proc.Kind = EProcKind::Lightning;
	Proc.A = Start;
	Proc.B = End;
	Proc.Lifetime = Duration;
	Proc.Color = LightningColor;
}

void UCRFxSubsystem::SpawnFlame(const FVector& GroundCenter, float Radius, float Height, float Duration)
{
	FProc& Proc = NextProc();
	Proc.Kind = EProcKind::Flame;
	Proc.A = GroundCenter;
	Proc.Size = Radius;
	Proc.Height = Height;
	Proc.Lifetime = Duration;
}

void UCRFxSubsystem::SpawnSmokePuff(const FVector& Location, float Size, const FVector& Direction, bool bRing)
{
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::Smoke;
		P.Shape = ECRFxShape::Sphere;
		P.Location = Location;
		P.Size = Size;
		P.Lifetime = UCRTuningSubsystem::GetSettings(this).MissileSmokeLifetime * FMath::FRandRange(0.85f, 1.15f);
	}
	if (bRing)
	{
		FPiece& P = NextPiece();
		P.Kind = EPieceKind::SmokeRing;
		P.Shape = ECRFxShape::Donut;
		P.Location = Location;
		// 도넛(XY 평면)의 축을 진행 방향으로
		P.Rotation = FRotationMatrix::MakeFromZ(Direction.GetSafeNormal()).ToQuat();
		P.Lifetime = UCRTuningSubsystem::GetSettings(this).MissileSmokeRingDuration;
		P.Color = SmokeWhite;
	}
}

void UCRFxSubsystem::SpawnSmear(const FVector& From, const FVector& To, float Diameter, const FLinearColor& Color)
{
	const int32 Fps = UCRTuningSubsystem::GetSettings(this).FxFps;
	FPiece& P = NextPiece();
	P.Kind = EPieceKind::Smear;
	P.Shape = ECRFxShape::Sphere;
	P.Origin = From;
	P.Location = To;
	P.Size = Diameter;
	P.Lifetime = Fps > 0 ? 1.f / Fps : 1.f / 30.f;
	P.Color = Color;
	// 그 사이 그림 한 장만 보이도록 다음 그리기에서 사라지게 한다
	bShapesDirty = true;
}

FQuat UCRFxSubsystem::FaceCamera(const FVector& Location, float Roll) const
{
	// 납작한 도형(YZ 평면)의 X축을 카메라로
	const FVector ToCamera = (CameraLocation - Location).GetSafeNormal();
	const FMatrix Basis = FRotationMatrix::MakeFromXZ(ToCamera.IsNearlyZero() ? FVector::ForwardVector : ToCamera,
		CameraRotation.GetUpVector());
	return Basis.ToQuat() * FQuat(FVector::ForwardVector, Roll);
}

void UCRFxSubsystem::UpdatePieces(float DeltaTime, float RealDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FxUpdatePieces);
	if (AlivePieces == 0)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	for (FPiece& P : Pieces)
	{
		if (!P.bAlive)
		{
			continue;
		}
		P.Age += DeltaTime;
		if (P.Age >= P.Delay + P.Lifetime)
		{
			P.bAlive = false;
			--AlivePieces;
			bShapesDirty = true;
			continue;
		}

		switch (P.Kind)
		{
		case EPieceKind::Debris:
		{
			P.Velocity.Z -= Tuning.DebrisGravity * DeltaTime;
			P.Location += P.Velocity * DeltaTime;
			P.Rotation = FQuat(P.SpinAxis, P.SpinSpeed * DeltaTime) * P.Rotation;
			const float HalfSize = P.Size * 50.f;
			if (P.Location.Z - HalfSize < P.GroundZ && P.Velocity.Z < 0.f)
			{
				P.Location.Z = P.GroundZ + HalfSize;
				P.Velocity.Z *= -Tuning.DebrisBounceUp;
				P.Velocity.X *= Tuning.DebrisBounceSide;
				P.Velocity.Y *= Tuning.DebrisBounceSide;
				P.SpinSpeed *= Tuning.DebrisBounceSpin;
			}
			break;
		}
		case EPieceKind::Fireball:
		{
			const float T = P.Age - P.Delay;
			if (T > P.Lifetime * Tuning.FireballRiseFrom)
			{
				P.Location.Z += Tuning.FireballRiseSpeed * DeltaTime; // 공기에 먹히듯 떠오르며 작아짐
			}
			break;
		}
		case EPieceKind::Smoke:
			P.Location.Z += Tuning.MissileSmokeRise * DeltaTime;
			break;
		case EPieceKind::DustPuff:
			P.Velocity *= FMath::Max(0.f, 1.f - 6.f * DeltaTime);
			P.Location += P.Velocity * DeltaTime;
			break;
		default:
			break;
		}
	}
}

void UCRFxSubsystem::UpdateProcs(float DeltaTime, float RealDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FxUpdateProcs);
	if (AliveProcs == 0)
	{
		return;
	}
	for (FProc& Proc : Procs)
	{
		if (!Proc.bAlive)
		{
			continue;
		}
		Proc.Age += Proc.bRealTime ? RealDeltaTime : DeltaTime;
		if (Proc.Age >= Proc.Lifetime)
		{
			Proc.bAlive = false;
			--AliveProcs;
			bShapesDirty = true;
			continue;
		}
		if (const USceneComponent* Follow = Proc.Follow.Get())
		{
			Proc.A = Follow->GetComponentTransform().TransformPosition(Proc.FollowOffset);
		}
	}
}

void UCRFxSubsystem::Emit(ECRFxShape Shape, const FTransform& Transform, const FLinearColor& Color, float Flat)
{
	FDrawList& List = ShapeLists[static_cast<int32>(Shape)];
	List.Transforms.Add(Transform);
	List.Custom.Append({ Color.R, Color.G, Color.B, Flat });
}

void UCRFxSubsystem::DrawPieces()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FxDrawPieces);
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	for (const FPiece& P : Pieces)
	{
		if (!P.bAlive || P.Age < P.Delay)
		{
			continue;
		}
		const float T = P.Age - P.Delay;
		const float Life = FMath::Max(P.Lifetime, 0.001f);
		const float Alpha = T / Life;

		switch (P.Kind)
		{
		case EPieceKind::Debris:
		{
			// 처음부터 원래 크기 (페이드인 없음), 수명 마지막 구간에 작아짐
			const float ShrinkFrom = FMath::Clamp(Tuning.DebrisShrinkFrom, 0.f, 0.99f);
			const float Shrink = Alpha < ShrinkFrom ? 1.f : 1.f - (Alpha - ShrinkFrom) / (1.f - ShrinkFrom);
			Emit(P.Shape, FTransform(P.Rotation, P.Location, FVector(P.Size * Shrink)), P.Color, 0.f);
			break;
		}
		case EPieceKind::Flash:
		{
			const bool bFirst = T < Tuning.ExplosionFlashTime;
			const float Scale = P.Size * Tuning.ExplosionFlashSize * (bFirst ? 1.f : 0.55f);
			Emit(P.Shape, FTransform(FQuat::Identity, P.Location, FVector(Scale)),
				bFirst ? FLinearColor(4.f, 4.f, 4.f) : FLinearColor(3.f, 2.4f, 0.6f), 1.f);
			break;
		}
		case EPieceKind::Fireball:
		{
			const float Grow = EaseOut(T / FMath::Max(Tuning.FireballInflateTime, 0.001f));
			const float RiseFrom = FMath::Clamp(Tuning.FireballRiseFrom, 0.f, 0.99f);
			const float Fade = Alpha < RiseFrom ? 1.f : 1.f - (Alpha - RiseFrom) / (1.f - RiseFrom);
			const bool bGlow = T < Tuning.FireGlowTime;
			const FLinearColor Color = bGlow ? FireGlow : T < Tuning.FireOrangeTime ? FireOrange : T < Tuning.FireRedTime ? FireRed : FireSmoke;
			Emit(P.Shape, FTransform(FQuat::Identity, P.Location, FVector(P.Size * Grow * Fade)), Color, bGlow ? 1.f : 0.f);
			break;
		}
		case EPieceKind::RingCube:
		{
			const float Spread = FMath::Lerp(Tuning.ShockSpreadStart, Tuning.ShockSpreadEnd, EaseOut(Alpha));
			const FVector Location = P.Origin + P.Velocity * Spread;
			Emit(P.Shape, FTransform(P.Rotation, Location, FVector(P.Size * (1.f - Alpha))), P.Color, 0.f);
			break;
		}
		case EPieceKind::Smoke:
		{
			const float Grow = FMath::Lerp(0.55f, 1.f, EaseOut(T / FMath::Max(Tuning.MissileSmokeInflateTime, 0.001f)));
			const float Fade = Alpha < 0.6f ? 1.f : 1.f - (Alpha - 0.6f) / 0.4f;
			const bool bGlow = T < Tuning.MissileSmokeGlowTime;
			const FLinearColor Color = bGlow ? SmokeGlow : Alpha < 0.45f ? SmokeWhite : SmokeGray;
			Emit(P.Shape, FTransform(FQuat::Identity, P.Location, FVector(P.Size * Grow * Fade)), Color, bGlow ? 1.f : 0.f);
			break;
		}
		case EPieceKind::SmokeRing:
		{
			// 작게(16cm) 시작해 넓어지며 얇아짐
			const float Diameter = FMath::Lerp(16.f, Tuning.MissileSmokeRingSize, EaseOut(Alpha));
			const float Thin = FMath::Lerp(1.f, 0.3f, Alpha);
			Emit(P.Shape, FTransform(P.Rotation, P.Location, FVector(Diameter / 100.f, Diameter / 100.f, 0.25f * Thin)), P.Color, 0.f);
			break;
		}
		case EPieceKind::DustRing:
		{
			const float Scale = FMath::Lerp(Tuning.DustRingStartScale, Tuning.DustRingEndScale, EaseOut(Alpha)) * P.Size;
			Emit(P.Shape, FTransform(FQuat::Identity, P.Location, FVector(Scale, Scale, 0.15f * (1.f - Alpha))), P.Color, 0.f);
			break;
		}
		case EPieceKind::DustPuff:
			Emit(P.Shape, FTransform(FQuat::Identity, P.Location, FVector(P.Size * (1.f - Alpha))), P.Color, 0.f);
			break;
		case EPieceKind::Smear:
		{
			const FVector Delta = P.Location - P.Origin;
			const float Length = Delta.Size() + P.Size;
			const FQuat Rotation = Delta.IsNearlyZero() ? FQuat::Identity : FRotationMatrix::MakeFromX(Delta).ToQuat();
			const float Thick = P.Size * Tuning.SmearThickness / 100.f;
			Emit(P.Shape, FTransform(Rotation, (P.Location + P.Origin) * 0.5f, FVector(Length / 100.f, Thick, Thick)), P.Color, 0.f);
			break;
		}
		}
	}
}

void UCRFxSubsystem::DrawProcs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FxDrawProcs);
	for (const FProc& Proc : Procs)
	{
		if (Proc.bAlive)
		{
			// 장마다 모양을 다시 뽑는다 (같은 장 안에서는 고정)
			FRandomStream Random(Proc.Seed * 7919 + FxDrawCount * 104729);
			DrawProc(Proc, Random);
		}
	}
}

void UCRFxSubsystem::DrawProc(const FProc& Proc, FRandomStream& Random)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const float Alpha = Proc.Age / FMath::Max(Proc.Lifetime, 0.001f);
	switch (Proc.Kind)
	{
	case EProcKind::HitStar:
	{
		// 흰색 → 색(1.25배) → 어둡고 작게, 장마다 회전 재추첨
		const int32 Stage = FMath::Min(FMath::FloorToInt(Alpha * 3.f), 2);
		const float White = Tuning.HitStarBrightness;
		const FLinearColor Color = Stage == 0 ? FLinearColor(White, White, White) : Stage == 1 ? Proc.Color * 1.25f : Proc.Color * 0.2f;
		const float Scale = Proc.Size / 50.f * (Stage == 0 ? 1.f : Stage == 1 ? Tuning.HitStarStage2Scale : Tuning.HitStarStage3Scale);
		Emit(ECRFxShape::Star8, FTransform(FaceCamera(Proc.A, Random.FRandRange(0.f, UE_TWO_PI)), Proc.A, FVector(Scale)), Color, 1.f);
		break;
	}
	case EProcKind::Sparkle:
	{
		// 짧게 튀어나와 돌며 작아짐
		const float PopTime = FMath::Clamp(Tuning.SparklePopTime, 0.001f, Proc.Lifetime * 0.9f);
		const float Rest = FMath::Max(Proc.Lifetime - PopTime, 0.001f);
		const float Pop = Proc.Age < PopTime ? EaseOut(Proc.Age / PopTime) : 1.f - (Proc.Age - PopTime) / Rest;
		const float Spin = FMath::DegreesToRadians(Tuning.SparkleSpin) * FMath::Clamp((Proc.Age - PopTime) / Rest, 0.f, 1.f);
		const float Scale = Proc.Size / 50.f * FMath::Max(Pop, 0.f);
		const float Bright = Tuning.SparkleBrightness;
		const FLinearColor Color(Bright, Bright, Bright * 0.9f);
		Emit(ECRFxShape::Star4, FTransform(FaceCamera(Proc.A, Spin), Proc.A, FVector(Scale)), Color, 1.f);
		Emit(ECRFxShape::Star4, FTransform(FaceCamera(Proc.A, Spin + UE_PI * 0.25f), Proc.A, FVector(Scale * 0.6f)), Color, 1.f);
		break;
	}
	case EProcKind::Kanada:
	{
		// 흰색 → 노랑 → 검은 가시 몇 개 → 끝
		const int32 Stage = FMath::Min(FMath::FloorToInt(Alpha * 3.f), 2);
		const int32 SpikeCount = FMath::Max(Tuning.KanadaCount, 1);
		for (int32 i = 0; i < SpikeCount; ++i)
		{
			const bool bShow = Stage < 2 || Random.FRand() < Tuning.KanadaBlackRatio;
			const float Angle = UE_TWO_PI * i / SpikeCount + Random.FRandRange(-0.2f, 0.2f);
			const float Length = Proc.Size * Random.FRandRange(Tuning.KanadaLengthMin, Tuning.KanadaLengthMax);
			const float Width = Random.FRandRange(Tuning.KanadaWidthMin, Tuning.KanadaWidthMax) * Proc.Size / 100.f;
			if (!bShow)
			{
				continue;
			}
			const FQuat Rotation = FaceCamera(Proc.A, Angle);
			const FVector Outward = Rotation.GetAxisZ();
			const FLinearColor Color = Stage == 0 ? FLinearColor(4.f, 4.f, 4.f) : Stage == 1 ? FLinearColor(3.f, 2.4f, 0.4f) : FLinearColor(0.02f, 0.02f, 0.03f);
			Emit(ECRFxShape::Spike, FTransform(Rotation, Proc.A + Outward * Proc.Size * 0.12f, FVector(1.f, Width, Length / 100.f)), Color, 1.f);
		}
		break;
	}
	case EProcKind::Lightning:
	{
		const FVector Delta = Proc.B - Proc.A;
		const float Length = Delta.Size();
		if (Length < 1.f)
		{
			break;
		}
		const FVector Dir = Delta / Length;
		const int32 MinPoints = FMath::Max(Tuning.LightningMinPoints, 2);
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Length / FMath::Max(Tuning.LightningSegmentLength, 1.f)), MinPoints, FMath::Max(Tuning.LightningMaxPoints, MinPoints));
		const float Jitter = FMath::Min(Length * Tuning.LightningJitterRatio, Tuning.LightningJitterMax);
		const float Thickness = FMath::Lerp(Tuning.LightningThickStart, Tuning.LightningThickEnd, Alpha);
		const FVector Perp = AnyPerpendicular(Dir);

		TArray<FVector, TInlineAllocator<65>> Points;
		for (int32 i = 0; i <= Count; ++i)
		{
			const float T = static_cast<float>(i) / Count;
			// 가운데일수록 크게, 양 끝은 고정
			const float Amount = FMath::Sin(T * UE_PI) * Jitter;
			const FVector Offset = Perp.RotateAngleAxis(Random.FRandRange(0.f, 360.f), Dir) * Random.FRandRange(0.f, Amount);
			Points.Add(Proc.A + Delta * T + Offset);
		}
		auto Segment = [this](const FVector& From, const FVector& To, float Thick, const FLinearColor& Color)
		{
			const FVector Seg = To - From;
			// 조각을 간격보다 조금 길게 해서 이음새 틈을 없앤다
			const float SegLength = Seg.Size() * 1.08f + Thick * 0.5f;
			Emit(ECRFxShape::Cube, FTransform(FRotationMatrix::MakeFromX(Seg).ToQuat(), (From + To) * 0.5f,
				FVector(SegLength / 100.f, Thick / 100.f, Thick / 100.f)), Color, 1.f);
		};
		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			Segment(Points[i], Points[i + 1], Thickness, Proc.Color);
		}
		// 가지: 본 줄기의 임의 점에서, 길이 비율 min~max, 굵기 절반
		for (int32 b = 0; b < Tuning.LightningBranchCount && Points.Num() > 2; ++b)
		{
			const int32 From = Random.RandRange(1, Points.Num() - 2);
			const FVector BranchDir = (Dir + Perp.RotateAngleAxis(Random.FRandRange(0.f, 360.f), Dir) * 0.9f).GetSafeNormal();
			const float BranchLength = Length * Random.FRandRange(Tuning.LightningBranchMin, Tuning.LightningBranchMax);
			const FVector Mid = Points[From] + BranchDir * BranchLength * 0.5f + Perp * Random.FRandRange(-0.15f, 0.15f) * BranchLength;
			const FVector End = Points[From] + BranchDir * BranchLength;
			Segment(Points[From], Mid, Thickness * 0.5f, Proc.Color);
			Segment(Mid, End, Thickness * 0.5f, Proc.Color);
		}
		// 양 끝 섬광 구체
		const float Flash = (Alpha < 0.5f ? 24.f : 14.f) / 100.f;
		Emit(ECRFxShape::Sphere, FTransform(FQuat::Identity, Proc.A, FVector(Flash * 0.7f)), FLinearColor(3.f, 4.f, 6.f), 1.f);
		Emit(ECRFxShape::Sphere, FTransform(FQuat::Identity, Proc.B, FVector(Flash)), FLinearColor(3.f, 4.f, 6.f), 1.f);
		break;
	}
	case EProcKind::Flame:
	{
		// 짧게 자라고 마지막 구간에 가라앉음
		float Grow = EaseOut(Proc.Age / FMath::Max(Tuning.FlameGrowTime, 0.001f));
		const float SinkFrom = FMath::Clamp(Tuning.FlameSinkFrom, 0.f, 0.99f);
		if (Alpha > SinkFrom)
		{
			Grow *= 1.f - (Alpha - SinkFrom) / (1.f - SinkFrom);
		}
		for (int32 i = 0; i < Tuning.FlameCount; ++i)
		{
			const float Dist = FMath::Sqrt(Random.FRand()) * Proc.Size;
			const float Angle = Random.FRandRange(0.f, UE_TWO_PI);
			const FVector Base = Proc.A + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Dist;
			// 가운데가 높다
			const float Ratio = FMath::Pow(1.f - Dist / FMath::Max(Proc.Size, 1.f), 0.7f) * Random.FRandRange(0.55f, 1.f);
			const float Height = Proc.Height * Ratio * Grow;
			const float Radius = Proc.Size * Random.FRandRange(Tuning.FlameSizeMin, Tuning.FlameSizeMax);
			const FRotator Tilt(Random.FRandRange(-Tuning.FlameTilt, Tuning.FlameTilt), Random.FRandRange(0.f, 360.f), Random.FRandRange(-Tuning.FlameTilt, Tuning.FlameTilt));
			// 높이 비율로 3색: 자홍 → 분홍 → 하늘색 끝
			const FLinearColor Color = Ratio > 0.7f ? FLinearColor(0.4f, 2.2f, 3.f) : Ratio > 0.4f ? FLinearColor(2.5f, 0.6f, 1.5f) : FLinearColor(2.2f, 0.15f, 1.6f);
			if (Height > 1.f)
			{
				Emit(ECRFxShape::Pyramid, FTransform(Tilt.Quaternion(), Base, FVector(Radius / 50.f, Radius / 50.f, Height / 100.f)), Color, 1.f);
			}
		}
		break;
	}
	}
}

void UCRFxSubsystem::FlushShapes()
{
	for (int32 s = 0; s < ShapeLists.Num(); ++s)
	{
		FDrawList& List = ShapeLists[s];
		UInstancedStaticMeshComponent* Component = ShapeComponents.IsValidIndex(s) ? ShapeComponents[s].Get() : nullptr;
		if (!Component)
		{
			List.Transforms.Reset();
			List.Custom.Reset();
			continue;
		}
		const int32 Need = List.Transforms.Num();
		if (Need == 0 && List.LastUsed == 0)
		{
			continue;
		}
		const int32 Have = Component->GetInstanceCount();
		if (Need > Have)
		{
			TArray<FTransform> Extra;
			Extra.Init(HiddenTransform, FMath::Max(Need - Have, 64));
			Component->AddInstances(Extra, false, true);
		}
		if (Need > 0)
		{
			Component->BatchUpdateInstancesTransforms(0, List.Transforms, true, false, true);
			for (int32 i = 0; i < Need; ++i)
			{
				Component->SetCustomData(i, TArrayView<const float>(&List.Custom[i * CRMaterialData::InstanceFloats], CRMaterialData::InstanceFloats), false);
			}
		}
		for (int32 i = Need; i < List.LastUsed; ++i)
		{
			Component->UpdateInstanceTransform(i, HiddenTransform, true, false, true);
		}
		List.LastUsed = Need;
		// 처음 만든 숨김 인스턴스(원점, 크기 0) 기준 경계로 컬링되지 않게 매번 다시 잡는다
		Component->UpdateBounds();
		Component->MarkRenderStateDirty();
		List.Transforms.Reset();
		List.Custom.Reset();
	}
}

void UCRFxSubsystem::FlushBlobs()
{
	UInstancedStaticMeshComponent* Component = BlobComponent;
	const int32 Need = BlobList.Transforms.Num();
	if (Component && (Need > 0 || BlobList.LastUsed > 0))
	{
		const int32 Have = Component->GetInstanceCount();
		if (Need > Have)
		{
			TArray<FTransform> Extra;
			Extra.Init(HiddenTransform, FMath::Max(Need - Have, 32));
			Component->AddInstances(Extra, false, true);
		}
		if (Need > 0)
		{
			Component->BatchUpdateInstancesTransforms(0, BlobList.Transforms, true, false, true);
			for (int32 i = 0; i < Need; ++i)
			{
				Component->SetCustomData(i, TArrayView<const float>(&BlobList.Custom[i], 1), false);
			}
		}
		for (int32 i = Need; i < BlobList.LastUsed; ++i)
		{
			Component->UpdateInstanceTransform(i, HiddenTransform, true, false, true);
		}
		BlobList.LastUsed = Need;
		Component->UpdateBounds();
		Component->MarkRenderStateDirty();
	}
	BlobList.Transforms.Reset();
	BlobList.Custom.Reset();
}

void UCRFxSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FxTick);
	Super::Tick(DeltaTime);
	if (!EnsureComponents())
	{
		return;
	}

	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		CameraLocation = Camera->GetCameraLocation();
		CameraRotation = Camera->GetCameraRotation().Quaternion();
	}

	const float RealDelta = FApp::GetDeltaTime();
	UpdatePieces(DeltaTime, RealDelta);
	UpdateProcs(DeltaTime, RealDelta);

	// 2코마: 그리는 프레임에만 인스턴스를 갱신 (스미어처럼 한 장짜리가 사라질 때도 그 프레임에 반영)
	if (bFxDrawFrame && (AlivePieces > 0 || AliveProcs > 0 || bShapesDirty))
	{
		DrawPieces();
		DrawProcs();
		FlushShapes();
		bShapesDirty = false;
	}
	FlushBlobs();
}
