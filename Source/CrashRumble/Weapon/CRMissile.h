#pragma once

#include "CoreMinimal.h"
#include "Core/CRActorPool.h"
#include "Weapon/CRSkill.h"
#include "GameFramework/Actor.h"
#include "CRMissile.generated.h"

class UStaticMeshComponent;

/**
 * 이타노 서커스 미사일. UCRActorPoolSubsystem에서 꺼내 쓰고 터지면 반납한다.
 * 느리게 출발해 가속하고, 선회 속도가 처음엔 작다가 점점 커져서 크게 돌다가 급하게 모인다.
 * 연기 궤적은 UCRFxSubsystem 조각이라 미사일이 터져도 남는다.
 */
UCLASS()
class CRASHRUMBLE_API ACRMissile : public AActor, public ICRPoolable
{
	GENERATED_BODY()

public:
	ACRMissile();

	/**
	 * Target이 있으면 쫓고, 없으면 TargetPoint로 간다 (이때 바닥에 빨간 범위 예고).
	 * 폭발 피해·반경·크레이터는 쏜 캐릭터의 런처 스탯 (Form = 액티브 / 패시브).
	 */
	void Launch(AActor* InShooter, const FVector& Direction, AActor* InTarget, const FVector& InTargetPoint,
		ECRSkillForm InForm = ECRSkillForm::Active);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPoolAcquired() override;
	virtual void OnPoolReleased() override;

protected:
	virtual void BeginPlay() override;

private:
	void Explode(const FVector& Location, const FVector& Normal, bool bGroundNear);
	void ReturnToPool();
	FVector GetTargetLocation_Missile() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Exhaust;

	TWeakObjectPtr<AActor> Shooter;
	TWeakObjectPtr<AActor> Target;
	FVector TargetPoint = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	FVector WobbleAxis = FVector::RightVector;
	float WobblePhase = 0.f;
	float Speed = 0.f;
	float Age = 0.f;
	float SmokeDistance = 0.f;
	int32 SmokeCount = 0;
	int32 MarkerId = INDEX_NONE;
	ECRSkillForm Form = ECRSkillForm::Active;
	bool bActive = false;
};
