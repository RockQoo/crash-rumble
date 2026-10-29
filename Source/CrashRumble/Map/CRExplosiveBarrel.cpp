#include "Map/CRExplosiveBarrel.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/CRFxSubsystem.h"
#include "Feedback/CRScreenFxSubsystem.h"
#include "GAS/CRCombat.h"
#include "Map/CRBlockArena.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BarrelRadius = 30.f;
	constexpr float BarrelHalfHeight = 45.f;
	const FLinearColor BodyColor(0.85f, 0.12f, 0.08f);
	const FLinearColor BandColor(1.f, 0.85f, 0.15f);
	constexpr float FlashTime = 0.1f;
	constexpr float FlashIntensity = 4.f;
}

ACRExplosiveBarrel::ACRExplosiveBarrel()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetRelativeScale3D(FVector(BarrelRadius * 2.f / 100.f, BarrelRadius * 2.f / 100.f, BarrelHalfHeight * 2.f / 100.f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCastShadow(false);
	if (CylinderFinder.Succeeded())
	{
		Body->SetStaticMesh(CylinderFinder.Object);
	}

	// 노란 띠 (위험 표시)
	Band = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Band"));
	Band->SetupAttachment(Body);
	Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Band->SetCastShadow(false);
	Band->SetRelativeLocation(FVector(0.f, 0.f, 15.f));
	Band->SetRelativeScale3D(FVector(1.05f, 1.05f, 0.18f));
	if (CylinderFinder.Succeeded())
	{
		Band->SetStaticMesh(CylinderFinder.Object);
	}
}

void ACRExplosiveBarrel::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial())
	{
		Body->SetMaterial(0, Toon);
		Body->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(BodyColor.R, BodyColor.G, BodyColor.B, 0.f));
		Band->SetMaterial(0, Toon);
		Band->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(BandColor.R, BandColor.G, BandColor.B, 0.f));
	}
	Health = UCRTuningSubsystem::GetSettings(this).BarrelHealth;
}

void ACRExplosiveBarrel::ApplyHit(int32 Damage, AActor* InInstigator)
{
	if (!bIntact || Damage <= 0)
	{
		return;
	}
	if (InInstigator)
	{
		LastInstigator = InInstigator;
	}
	Health -= Damage;
	FlashTimeLeft = FlashTime;
	Body->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, FlashIntensity);
	if (Health <= 0)
	{
		Explode();
	}
}

void ACRExplosiveBarrel::Explode()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	bIntact = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	const FVector Center = GetActorLocation();
	const float GroundZ = Center.Z - BarrelHalfHeight;
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		if (Tuning.bSphereExplosion)
		{
			Fx->SpawnSphereExplosion(Center, Tuning.BarrelBlastRadius * 0.45f, GroundZ);
		}
		if (Tuning.bCubeDebris)
		{
			Fx->SpawnDebris(Center, FVector::UpVector, 14, BodyColor, FLinearColor(0.3f, 0.3f, 0.32f), 1.3f, GroundZ);
		}
		if (Tuning.bKanada)
		{
			Fx->SpawnKanada(Center, 120.f);
		}
	}

	// 주변 눈사람 피해 (쏜 사람이 공격자), 블럭 크레이터
	AActor* Shooter = LastInstigator.Get();
	if (Shooter)
	{
		CRCombat::ApplyRadialHit(Shooter, ECRWeaponType::Missile, Center, Tuning.BarrelBlastRadius);
		const APawn* ShooterPawn = Cast<APawn>(Shooter);
		if (ShooterPawn && ShooterPawn->IsPlayerControlled())
		{
			if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(this))
			{
				Screen->RequestImpactFrame();
			}
			if (const ACRCharacter* Snowman = Cast<ACRCharacter>(ShooterPawn); Snowman && Tuning.bCameraShake && Snowman->GetViewComponent())
			{
				Snowman->GetViewComponent()->AddShake(0.8f);
			}
		}
	}
	if (ACRBlockArena* Arena = ACRBlockArena::Find(this))
	{
		Arena->DestroySphere(Center, Tuning.MissileCraterRadius);
	}

	// 연쇄: 근처 드럼통이 조금 늦게 따라 터진다
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Tuning.BarrelBlastRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CRBarrelChain), false, this));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACRExplosiveBarrel* Other = Cast<ACRExplosiveBarrel>(Overlap.GetActor());
		if (Other && Other->IsIntact() && !Other->ExplodeTimer.IsValid())
		{
			TWeakObjectPtr<ACRExplosiveBarrel> WeakOther = Other;
			TWeakObjectPtr<AActor> WeakShooter = Shooter;
			Other->LastInstigator = Shooter;
			GetWorldTimerManager().SetTimer(Other->ExplodeTimer, FTimerDelegate::CreateLambda([WeakOther, WeakShooter]()
			{
				if (ACRExplosiveBarrel* Barrel = WeakOther.Get())
				{
					Barrel->ExplodeTimer.Invalidate();
					Barrel->ApplyHit(1000, WeakShooter.Get());
				}
			}), FMath::FRandRange(0.12f, 0.22f), false);
		}
	}

	GetWorldTimerManager().SetTimer(RestoreTimer, this, &ACRExplosiveBarrel::Restore, FMath::Max(Tuning.BarrelRespawnDelay, 0.1f), false);
}

void ACRExplosiveBarrel::Restore()
{
	// 그 자리에 캐릭터가 있으면 조금 미룬다
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	if (GetWorld()->OverlapAnyTestByObjectType(GetActorLocation(), FQuat::Identity, Objects,
		FCollisionShape::MakeCapsule(BarrelRadius, BarrelHalfHeight), FCollisionQueryParams(SCENE_QUERY_STAT(CRBarrelRestore), false, this)))
	{
		GetWorldTimerManager().SetTimer(RestoreTimer, this, &ACRExplosiveBarrel::Restore, 0.5f, false);
		return;
	}
	bIntact = true;
	Health = UCRTuningSubsystem::GetSettings(this).BarrelHealth;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		Fx->SpawnSparkle(GetActorLocation() + FVector(0.f, 0.f, BarrelHalfHeight), 30.f);
	}
}

void ACRExplosiveBarrel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (FlashTimeLeft > 0.f)
	{
		FlashTimeLeft -= DeltaSeconds;
		const float Alpha = FMath::Clamp(FlashTimeLeft / FlashTime, 0.f, 1.f);
		Body->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, FlashIntensity * Alpha * Alpha);
	}
}
