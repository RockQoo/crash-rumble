#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CRExplosiveBarrel.generated.h"

class UStaticMeshComponent;

/**
 * 훈련장 폭발 드럼통. 맞으면 HP가 줄고, 0이 되면 터져 주변 눈사람에 피해 + 블럭 크레이터.
 * 근처 드럼통은 조금 늦게 따라 터진다 (연쇄). 일정 시간 뒤 같은 자리에 다시 생긴다 (액터 재사용).
 * 콜리전 오브젝트 타입은 Pawn: 칼 스윕, 투사체 오버랩, 범위 피해가 눈사람처럼 찾는다.
 */
UCLASS()
class CRASHRUMBLE_API ACRExplosiveBarrel : public AActor
{
	GENERATED_BODY()

public:
	ACRExplosiveBarrel();

	/** 무기에 맞음. Instigator = 쏜 쪽 (폭발 피해의 공격자). */
	void ApplyHit(int32 Damage, AActor* InInstigator);
	bool IsIntact() const { return bIntact; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void Explode();
	void Restore();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Band;

	TWeakObjectPtr<AActor> LastInstigator;
	int32 Health = 0;
	bool bIntact = true;
	float FlashTimeLeft = 0.f;
	FTimerHandle ExplodeTimer;
	FTimerHandle RestoreTimer;
};
