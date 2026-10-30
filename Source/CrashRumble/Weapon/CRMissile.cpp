#include "Weapon/CRMissile.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRBenchmark.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/CRFxSubsystem.h"
#include "Feedback/CRScreenFxSubsystem.h"
#include "GAS/CRCombat.h"
#include "Map/CRBlockArena.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float MaxLifetime = 4.f;    // 4초가 지나면 자폭
	constexpr float HitDistance = 35.f;   // 목표 35cm 이내면 명중
	constexpr float ExplosionVisualRadius = 60.f;
	constexpr float WobbleDegrees = 18.f;
	constexpr float GroundNearDistance = 120.f;

	const FLinearColor MissileBodyColor(0.85f, 0.87f, 0.92f);
	const FLinearColor MissileExhaustColor(3.f, 1.5f, 0.3f);
	const FLinearColor MarkerColor(2.5f, 0.12f, 0.08f);
}

ACRMissile::ACRMissile()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	Body->SetRelativeScale3D(FVector(0.28f, 0.08f, 0.08f));
	if (CubeFinder.Succeeded())
	{
		Body->SetStaticMesh(CubeFinder.Object);
	}

	// 원뿔 불꽃: 끝이 뒤를 향하게 (원뿔 끝 = +Z)
	Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Exhaust"));
	Exhaust->SetupAttachment(Root);
	Exhaust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Exhaust->SetCastShadow(false);
	Exhaust->SetRelativeLocationAndRotation(FVector(-20.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
	Exhaust->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.16f));
	if (ConeFinder.Succeeded())
	{
		Exhaust->SetStaticMesh(ConeFinder.Object);
	}
}

void ACRMissile::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial())
	{
		Body->SetMaterial(0, Toon);
		Body->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(MissileBodyColor.R, MissileBodyColor.G, MissileBodyColor.B, 0.f));
		Exhaust->SetMaterial(0, Toon);
		Exhaust->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(MissileExhaustColor.R, MissileExhaustColor.G, MissileExhaustColor.B, 1.f));
	}
}

void ACRMissile::Launch(AActor* InShooter, const FVector& InDirection, AActor* InTarget, const FVector& InTargetPoint, ECRSkillForm InForm)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	Shooter = InShooter;
	Form = InForm;
	Target = InTarget;
	TargetPoint = InTargetPoint;
	Direction = InDirection.GetSafeNormal();
	Speed = Tuning.MissileStartSpeed;
	Age = 0.f;
	SmokeDistance = 0.f;
	SmokeCount = 0;
	WobblePhase = FMath::FRandRange(0.f, UE_TWO_PI);
	WobbleAxis = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (WobbleAxis.IsNearlyZero())
	{
		WobbleAxis = FVector::RightVector;
	}
	SetActorRotation(Direction.Rotation());

	// 지점 사격: 떨어질 자리에 빨간 범위 (거리로 대략 도착 시간을 잡고, 터지면 바로 지운다)
	MarkerId = INDEX_NONE;
	if (!InTarget)
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			const float Radius = CRSkill::GetStats(InShooter, ECRWeaponType::Missile, Form).Get(ECRStat::Radius);
			const float Eta = FVector::Dist(GetActorLocation(), InTargetPoint) / FMath::Max(Tuning.MissileMaxSpeed * 0.6f, 100.f) + 0.4f;
			MarkerId = Fx->SpawnGroundMarker(InTargetPoint, Radius, FMath::Min(Eta, MaxLifetime), MarkerColor);
		}
	}
}

FVector ACRMissile::GetTargetLocation_Missile() const
{
	if (const AActor* Actor = Target.Get())
	{
		const ACRCharacter* Snowman = Cast<ACRCharacter>(Actor);
		if (!Snowman || (!Snowman->IsDead() && !Snowman->IsInPool()))
		{
			return Actor->GetActorLocation();
		}
	}
	return TargetPoint;
}

void ACRMissile::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_MissileTick);
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	Age += DeltaSeconds;
	if (Age >= MaxLifetime)
	{
		Explode(GetActorLocation(), FVector::UpVector, false);
		return;
	}

	// 선회 속도: 처음엔 크게 돌고 나중에 급하게 모인다
	const float Ramp = FMath::Clamp(Age / FMath::Max(Tuning.MissileTurnRampTime, 0.01f), 0.f, 1.f);
	const float TurnRate = FMath::DegreesToRadians(FMath::Lerp(Tuning.MissileTurnStart, Tuning.MissileTurnEnd, Ramp));
	const FVector Goal = GetTargetLocation_Missile();
	const FVector Desired = (Goal - GetActorLocation()).GetSafeNormal();
	const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Desired), -1.f, 1.f));
	if (Angle > KINDA_SMALL_NUMBER)
	{
		const float Step = FMath::Min(Angle, TurnRate * DeltaSeconds);
		FVector Axis = FVector::CrossProduct(Direction, Desired).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			Axis = WobbleAxis;
		}
		Direction = FQuat(Axis, Step).RotateVector(Direction).GetSafeNormal();
	}

	// 초반에만 좌우로 흔들고, 유도가 세지면 사라진다
	WobblePhase += DeltaSeconds * 14.f;
	const float Wobble = FMath::DegreesToRadians(WobbleDegrees) * (1.f - Ramp) * FMath::Sin(WobblePhase);
	const FVector FlyDirection = FQuat(FVector::CrossProduct(Direction, WobbleAxis).GetSafeNormal(), Wobble * DeltaSeconds * 6.f).RotateVector(Direction);
	Direction = FlyDirection.GetSafeNormal();

	Speed = FMath::Min(Tuning.MissileMaxSpeed, Speed + Tuning.MissileAcceleration * DeltaSeconds);
	const FVector Start = GetActorLocation();
	const FVector End = Start + Direction * Speed * DeltaSeconds;

	// 지형이나 캐릭터에 닿으면 터진다
	const AActor* ShooterActorForHit = Shooter.Get();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRMissile), false, this);
	if (AActor* ShooterActor = Shooter.Get())
	{
		Params.AddIgnoredActor(ShooterActor);
	}
	FHitResult Hit;
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, CRCombat::GetWeaponQueryParams(),
		FCollisionShape::MakeSphere(8.f), Params))
	{
		const ACRCharacter* HitSnowman = Cast<ACRCharacter>(Hit.GetActor());
		// 쓰러진 눈사람과 아군(같은 팀, 도플갱어)은 지나간다
		if (!HitSnowman || (!HitSnowman->IsDead() && CRCombat::IsHostile(ShooterActorForHit, HitSnowman)))
		{
			Explode(Hit.ImpactPoint, Hit.ImpactNormal, !HitSnowman);
			return;
		}
	}
	SetActorLocationAndRotation(End, Direction.Rotation());
	if (FVector::DistSquared(End, Goal) < FMath::Square(HitDistance))
	{
		Explode(End, -Direction, false);
		return;
	}

	// 연기 궤적: 일정 간격마다 한 덩이, 10개마다 진행 방향에 수직인 작은 링
	SmokeDistance += FVector::Dist(Start, End);
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		// 간격마다 한 덩이, 링은 N개마다 (튜닝 "미사일 연기" 탭)
		const float Spacing = FMath::Max(Tuning.MissileSmokeSpacing, 1.f);
		while (SmokeDistance >= Spacing)
		{
			SmokeDistance -= Spacing;
			const FVector Puff = End - Direction * SmokeDistance;
			++SmokeCount;
			const bool bRing = Tuning.MissileSmokeRingEvery > 0 && SmokeCount % Tuning.MissileSmokeRingEvery == 0;
			Fx->SpawnSmokePuff(Puff, Tuning.MissileSmokeSize * FMath::FRandRange(0.8f, 1.2f), Direction, bRing);
		}
	}
}

void ACRMissile::Explode(const FVector& Location, const FVector& Normal, bool bGroundHit)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	AActor* ShooterActor = Shooter.Get();
	const FCRSkillStats Stats = CRSkill::GetStats(ShooterActor, ECRWeaponType::Missile, Form);
	if (MarkerId != INDEX_NONE)
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			Fx->EndGroundMarker(MarkerId);
		}
		MarkerId = INDEX_NONE;
	}

	// 바닥 근처면 잔해를 바닥에서 위로
	float GroundZ = Location.Z - GroundNearDistance;
	bool bGroundNear = bGroundHit;
	FHitResult Ground;
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->LineTraceSingleByObjectType(Ground, Location + FVector(0.f, 0.f, 10.f), Location - FVector(0.f, 0.f, GroundNearDistance),
		GroundTypes, FCollisionQueryParams(SCENE_QUERY_STAT(CRMissileGround), false, this)))
	{
		GroundZ = Ground.ImpactPoint.Z;
		bGroundNear = true;
	}

	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		if (Tuning.bSphereExplosion)
		{
			Fx->SpawnSphereExplosion(Location, ExplosionVisualRadius, GroundZ);
		}
		if (Tuning.bCubeDebris)
		{
			const FVector DebrisOrigin = bGroundNear ? FVector(Location.X, Location.Y, GroundZ) : Location;
			Fx->SpawnDebris(DebrisOrigin, bGroundNear ? FVector::UpVector : Normal, 14,
				FLinearColor(0.55f, 0.55f, 0.6f), FLinearColor(0.35f, 0.33f, 0.38f), 1.f, GroundZ);
		}
		if (Tuning.bKanada)
		{
			Fx->SpawnKanada(Location, 150.f);
		}
	}

	// 크레이터: 반경 안 블럭을 전부 지운다
	if (ACRBlockArena* Arena = ACRBlockArena::Find(this))
	{
		// 벤치마크 중이면 벤치마크 크레이터, 아니면 런처 스탯 (업그레이드로 더 깊게)
		const UCRBenchmarkSubsystem* Bench = UCRBenchmarkSubsystem::Get(this);
		Arena->DestroySphere(Location, Bench && Bench->IsActive() ? CRCombat::GetCraterRadius(this) : Stats.Get(ECRStat::Crater));
	}

	if (ShooterActor)
	{
		CRCombat::ApplyRadialHit(ShooterActor, ECRWeaponType::Missile, Location, Stats.Get(ECRStat::Radius), -1.f, Form);

		const ACRCharacter* ShooterSnowman = Cast<ACRCharacter>(ShooterActor);
		if (ShooterSnowman && ShooterSnowman->IsPlayerControlled())
		{
			if (Tuning.bCameraShake && ShooterSnowman->GetViewComponent())
			{
				ShooterSnowman->GetViewComponent()->AddShake(Tuning.MissileShake * 0.5f);
			}
			if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(this))
			{
				Screen->RequestImpactFrame();
			}
		}
	}
	ReturnToPool();
}

void ACRMissile::ReturnToPool()
{
	if (!bActive)
	{
		return;
	}
	if (UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this))
	{
		Pool->Release(this);
	}
	else
	{
		OnPoolReleased();
	}
}

void ACRMissile::OnPoolAcquired()
{
	bActive = true;
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
}

void ACRMissile::OnPoolReleased()
{
	bActive = false;
	Target.Reset();
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
}
