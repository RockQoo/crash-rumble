#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "UObject/ObjectKey.h"
#include "CRActorPool.generated.h"

UINTERFACE(MinimalAPI)
class UCRPoolable : public UInterface
{
	GENERATED_BODY()
};

/** 풀에서 재사용되는 액터. 꺼낼 때/반납할 때 상태를 켜고 끈다. */
class ICRPoolable
{
	GENERATED_BODY()

public:
	virtual void OnPoolAcquired() {}
	virtual void OnPoolReleased() {}
};

USTRUCT()
struct FCRActorPoolList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;
};

/**
 * 클래스별 액터 풀. SpawnActor/Destroy 반복 대신 Acquire/Release를 쓴다.
 * 풀이 비어 있으면 새로 스폰해서 늘린다.
 */
UCLASS()
class CRASHRUMBLE_API UCRActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRActorPoolSubsystem* Get(const UObject* WorldContext);

	AActor* Acquire(UClass* Class, const FTransform& Transform);

	template <typename T>
	T* Acquire(const FTransform& Transform)
	{
		return Cast<T>(Acquire(T::StaticClass(), Transform));
	}

	void Release(AActor* Actor);
	void Prewarm(UClass* Class, int32 Count);

	/** 풀이 지금까지 실제로 스폰한 액터 수 (재사용이 잘 되면 거의 늘지 않는다). */
	int32 GetTotalSpawned() const { return TotalSpawned; }

	/** 클래스별: 지금 꺼내 쓰는 중 / 풀에서 대기 중. */
	int32 GetActiveCount(UClass* Class) const { return ActiveCounts.FindRef(Class); }
	int32 GetFreeCount(UClass* Class) const;

private:
	AActor* SpawnPooled(UClass* Class);

	UPROPERTY()
	TMap<TObjectPtr<UClass>, FCRActorPoolList> FreeActors;

	TMap<UClass*, int32> ActiveCounts;
	/** 지금 빌려준 액터. 반납은 여기 있는 것만 받는다 (중복 반납, 다른 풀/월드 액터 거부). */
	TSet<TObjectKey<AActor>> LentActors;
	int32 TotalSpawned = 0;
};
