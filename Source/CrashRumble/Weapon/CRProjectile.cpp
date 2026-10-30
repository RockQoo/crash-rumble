#include "Weapon/CRProjectile.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Character/CRCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/CRCombat.h"
#include "Map/CRBlockArena.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BaseRadius = 8.f;
	const FLinearColor ProjectileColor(1.f, 0.6f, 0.2f);
}

ACRProjectile::ACRProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(BaseRadius);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore); // 탄끼리 무시
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 캐릭터는 통과하며 판정 (관통)
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetRelativeScale3D(FVector(BaseRadius * 2.f / 100.f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		Mesh->SetStaticMesh(SphereFinder.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f;
}

void ACRProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentHit.AddDynamic(this, &ACRProjectile::HandleHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ACRProjectile::HandleOverlap);
	if (UMaterialInterface* Glow = UCRGameSettings::LoadGlowMaterial())
	{
		Mesh->SetMaterial(0, Glow);
		Mesh->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(ProjectileColor.R, ProjectileColor.G, ProjectileColor.B, 1.f));
		Mesh->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, 8.f);
	}
}

void ACRProjectile::Launch(AActor* InShooter, const FVector& Direction, ECRSkillForm InForm)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FCRSkillStats Stats = CRSkill::GetStats(InShooter, ECRWeaponType::Projectile, InForm);
	Form = InForm;

	if (AActor* Previous = Shooter.Get())
	{
		Collision->IgnoreActorWhenMoving(Previous, false);
	}
	Shooter = InShooter;
	Collision->IgnoreActorWhenMoving(InShooter, true);

	const float Size = FMath::Max(Stats.Get(ECRStat::Size), 0.05f);
	Collision->SetSphereRadius(BaseRadius * Size);
	Mesh->SetRelativeScale3D(FVector(BaseRadius * 2.f * Size / 100.f));

	LifeLeft = Tuning.ProjectileLifetime;
	PierceLeft = FMath::Max(Stats.GetInt(ECRStat::Pierce), 0);
	HitActors.Reset();
	Movement->ProjectileGravityScale = Tuning.ProjectileGravity;
	Movement->SetUpdatedComponent(Collision);
	Movement->Velocity = Direction.GetSafeNormal() * FMath::Max(Stats.Get(ECRStat::Speed), 100.f);
	Movement->Activate(true);
	Movement->UpdateComponentVelocity();
}

void ACRProjectile::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_ProjectileTick);
	Super::Tick(DeltaSeconds);

	if (bActive)
	{
		LifeLeft -= DeltaSeconds;
		if (LifeLeft <= 0.f)
		{
			ReturnToPool();
		}
	}
}

void ACRProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bActive)
	{
		return;
	}

	// 벽/바닥에 막힘 (캐릭터는 오버랩으로 처리). 블럭이면 HP 1
	CRCombat::DamageWorldByGun(Hit, Shooter.Get());
	if (UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(this))
	{
		Feedback->SpawnImpactParticles(Hit.ImpactPoint, Hit.ImpactNormal, ProjectileColor, 0.6f);
	}
	ReturnToPool();
}

void ACRProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bActive || !OtherActor || OtherActor == Shooter.Get() || HitActors.Contains(OtherActor))
	{
		return;
	}

	FHitResult Hit = SweepResult;
	if (!bFromSweep)
	{
		Hit = FHitResult(OtherActor, OtherComp, GetActorLocation(), -GetVelocity().GetSafeNormal());
	}

	if (CRCombat::ApplyWeaponHit(Shooter.Get(), OtherActor, ECRWeaponType::Projectile, Hit, Form))
	{
		HitActors.Add(OtherActor);
		if (PierceLeft-- <= 0)
		{
			ReturnToPool();
		}
	}
}

void ACRProjectile::ReturnToPool()
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

void ACRProjectile::OnPoolAcquired()
{
	bActive = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void ACRProjectile::OnPoolReleased()
{
	bActive = false;
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}
