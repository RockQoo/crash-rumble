#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRLootSubsystem.generated.h"

class ACRCharacter;
class UInstancedStaticMeshComponent;

/**
 * 몬스터가 떨어뜨리는 골드 동전 (임시 오브젝트: 노란 납작 원기둥).
 * 동전은 고정 크기 버퍼(풀)에 두고 인스턴스 메시 하나로 그린다 (액터 생성 없음).
 * 흩어져 튕기다가 플레이어·도플갱어가 자석 반경에 들어오면 빨려가 주인 인벤토리 골드에 더해진다.
 */
UCLASS()
class CRASHRUMBLE_API UCRLootSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRLootSubsystem* Get(const UObject* WorldContext);

	/** 쓰러진 캐릭터가 골드를 떨굴지 정해서 떨군다 (웨이브·생성 몬스터, 설정이 켜져 있으면 훈련장 허수아비). */
	void DropForDeath(const ACRCharacter* Victim);
	void SpawnCoins(const FVector& Location, int32 Count, int32 ValueEach);
	void ClearCoins();
	int32 GetCoinCount() const { return AliveCoins; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	struct FCoin
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float GroundZ = 0.f;
		float Age = 0.f;
		float Spin = 0.f;
		int32 Value = 1;
		bool bMagnet = false;
		bool bAlive = false;
	};

	bool EnsureComponent();
	void Collect(FCoin& Coin, ACRCharacter* Collector);

	TArray<FCoin> Coins;
	int32 NextCoin = 0;
	int32 AliveCoins = 0;
	int32 LastDrawn = 0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CoinActor;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> CoinMesh;
};
