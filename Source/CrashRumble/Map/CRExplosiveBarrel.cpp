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
	const FLinearColor BarrelBodyColor(0.85f, 0.12f, 0.08f);
	const FLinearColor BarrelBandColor(1.f, 0.85f, 0.15f);
	constexpr float BarrelFlashTime = 0.1f;
	constexpr float BarrelFlashIntensity = 4.f;
	// 폭발 크기 배율 1일 때의 기준 크기
	constexpr float BarrelFireballRadius = 100.f;
	constexpr float BarrelDebrisCount = 14.f;
	constexpr float BarrelKanadaSize = 120.f;
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
		Body->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(BarrelBodyColor.R, BarrelBodyColor.G, BarrelBodyColor.B, 0.f));
		Band->SetMaterial(0, Toon);
		Band->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(BarrelBandColor.R, BarrelBandColor.G, BarrelBandColor.B, 0.f));
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
	FlashTimeLeft = BarrelFlashTime;
	Body->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, BarrelFlashIntensity);
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
		const float Scale = Tuning.BarrelExplosionScale;
		if (Tuning.bSphereExplosion)
		{
			Fx->SpawnSphereExplosion(Center, BarrelFireballRadius * Scale, GroundZ);
		}
		if (Tuning.bCubeDebris)
		{
			Fx->SpawnDebris(Center, FVector::UpVector, FMath::RoundToInt(BarrelDebrisCount * Scale), BarrelBodyColor, FLinearColor(0.3f, 0.3f, 0.32f),
				1.3f * FMath::Sqrt(Scale), GroundZ);
		}
		if (Tuning.bKanada)
		{
			Fx->SpawnKanada(Center, BarrelKanadaSize * Scale);
		}
		// 연기 기둥 (레퍼런스 battle.gif): 덩이들이 서로 겹친 한 덩어리로 세로로 쌓여 솟고, 천천히 떠오른다.
		// 위쪽 덩이일수록 빨리 솟고 오래 남아서, 사라질 땐 아래부터 하나씩 작아지며 기둥이 가늘어진다
		const int32 SmokeCount = FMath::Max(Tuning.BarrelSmokeCount, 0);
		for (int32 i = 0; i < SmokeCount; ++i)
		{
			const float Height = SmokeCount > 1 ? float(i) / (SmokeCount - 1) : 0.f;
			const FVector2D Disk = FMath::RandPointInCircle(Tuning.BarrelSmokeSpread * FMath::Lerp(1.f, 0.6f, Height));
			const FVector Puff(Center.X + Disk.X, Center.Y + Disk.Y, GroundZ + 40.f + Height * Tuning.BarrelSmokeColumnHeight);
			const FVector Out = FVector(Disk.X, Disk.Y, 0.f).GetSafeNormal() * Tuning.BarrelSmokeOutSpeed * FMath::FRandRange(0.3f, 1.f);
			const FVector Up(0.f, 0.f, Tuning.BarrelSmokeUpSpeed * FMath::Lerp(0.4f, 1.f, Height) * FMath::FRandRange(0.85f, 1.15f));
			const float Life = Tuning.BarrelSmokeLifetime * FMath::Lerp(0.55f, 1.15f, Height);
			Fx->SpawnBigSmoke(Puff, Tuning.BarrelSmokeSize * FMath::FRandRange(0.75f, 1.3f), Up + Out,
				FMath::FRandRange(0.f, Tuning.BarrelSmokeDelay), Life);
		}
	}

	// 주변 눈사람 피해 (쏜 사람이 공격자), 블럭 크레이터
	AActor* Shooter = LastInstigator.Get();
	if (Shooter)
	{
		CRCombat::ApplyRadialHit(Shooter, ECRWeaponType::Missile, Center, Tuning.BarrelBlastRadius, Tuning.BarrelDamage);
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
		Arena->DestroySphere(Center, Tuning.BarrelCraterRadius);
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
		const float Alpha = FMath::Clamp(FlashTimeLeft / BarrelFlashTime, 0.f, 1.f);
		Body->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, BarrelFlashIntensity * Alpha * Alpha);
	}
}
