#include "Weapon/SBProjectile.h"

#include "Character/SBSnowmanCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SBGameSettings.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Feedback/SBFeedbackSubsystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GAS/SBCombat.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float BaseRadius = 8.f;
	const FLinearColor ProjectileColor(1.f, 0.6f, 0.2f);
}

ASBProjectile::ASBProjectile()
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
	Collision->SetGenerateOverlapEvents(false);
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

void ASBProjectile::BeginPlay()
{
	Super::BeginPlay();

	Collision->OnComponentHit.AddDynamic(this, &ASBProjectile::HandleHit);
	if (UMaterialInterface* Glow = USBGameSettings::LoadGlowMaterial())
	{
		Mesh->SetMaterial(0, Glow);
		Mesh->SetCustomPrimitiveDataVector4(SBMaterialData::ColorIndex, FVector4(ProjectileColor.R, ProjectileColor.G, ProjectileColor.B, 1.f));
		Mesh->SetCustomPrimitiveDataFloat(SBMaterialData::FlashIndex, 8.f);
	}
}

void ASBProjectile::Launch(AActor* InShooter, const FVector& Direction)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

	if (AActor* Previous = Shooter.Get())
	{
		Collision->IgnoreActorWhenMoving(Previous, false);
	}
	Shooter = InShooter;
	Collision->IgnoreActorWhenMoving(InShooter, true);

	const float Size = FMath::Max(Tuning.ProjectileSize, 0.05f);
	Collision->SetSphereRadius(BaseRadius * Size);
	Mesh->SetRelativeScale3D(FVector(BaseRadius * 2.f * Size / 100.f));

	LifeLeft = Tuning.ProjectileLifetime;
	Movement->ProjectileGravityScale = Tuning.ProjectileGravity;
	Movement->SetUpdatedComponent(Collision);
	Movement->Velocity = Direction.GetSafeNormal() * Tuning.ProjectileSpeed;
	Movement->Activate(true);
	Movement->UpdateComponentVelocity();
}

void ASBProjectile::Tick(float DeltaSeconds)
{
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

void ASBProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bActive)
	{
		return;
	}

	const bool bHitSnowman = SBCombat::ApplyWeaponHit(Shooter.Get(), OtherActor, ESBWeaponType::Projectile, Hit);
	if (!bHitSnowman)
	{
		if (USBFeedbackSubsystem* Feedback = USBFeedbackSubsystem::Get(this))
		{
			Feedback->SpawnImpactParticles(Hit.ImpactPoint, Hit.ImpactNormal, ProjectileColor, 0.6f);
		}
	}
	ReturnToPool();
}

void ASBProjectile::ReturnToPool()
{
	if (!bActive)
	{
		return;
	}
	if (USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this))
	{
		Pool->Release(this);
	}
	else
	{
		OnPoolReleased();
	}
}

void ASBProjectile::OnPoolAcquired()
{
	bActive = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
}

void ASBProjectile::OnPoolReleased()
{
	bActive = false;
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
}
