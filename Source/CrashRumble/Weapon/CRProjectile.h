#pragma once

#include "CoreMinimal.h"
#include "Core/CRActorPool.h"
#include "GameFramework/Actor.h"
#include "CRProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/** 투사체총 탄. UCRActorPoolSubsystem에서 꺼내 쓰고, 맞거나 수명이 다하면 반납한다. */
UCLASS()
class CRASHRUMBLE_API ACRProjectile : public AActor, public ICRPoolable
{
	GENERATED_BODY()

public:
	ACRProjectile();

	void Launch(AActor* InShooter, const FVector& Direction);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPoolAcquired() override;
	virtual void OnPoolReleased() override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ReturnToPool();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	TWeakObjectPtr<AActor> Shooter;
	float LifeLeft = 0.f;
	int32 PierceLeft = 0;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	bool bActive = false;
};
