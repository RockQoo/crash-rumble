#pragma once

#include "CoreMinimal.h"
#include "Core/SBActorPool.h"
#include "GameFramework/Actor.h"
#include "SBProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 투사체총 탄. USBActorPoolSubsystem에서 꺼내 쓰고, 맞거나 수명이 다하면 반납한다. */
UCLASS()
class SNOWBATTLE_API ASBProjectile : public AActor, public ISBPoolable
{
	GENERATED_BODY()

public:
	ASBProjectile();

	void Launch(AActor* InShooter, const FVector& Direction);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPoolAcquired() override;
	virtual void OnPoolReleased() override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ReturnToPool();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	TWeakObjectPtr<AActor> Shooter;
	float LifeLeft = 0.f;
	bool bActive = false;
};
