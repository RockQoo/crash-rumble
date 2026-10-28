#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "SBActorPool.generated.h"

UINTERFACE(MinimalAPI)
class USBPoolable : public UInterface
{
	GENERATED_BODY()
};

/** 풀에서 재사용되는 액터. 꺼낼 때/반납할 때 상태를 켜고 끈다. */
class ISBPoolable
{
	GENERATED_BODY()

public:
	virtual void OnPoolAcquired() {}
	virtual void OnPoolReleased() {}
};

USTRUCT()
struct FSBActorPoolList
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
class SNOWBATTLE_API USBActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USBActorPoolSubsystem* Get(const UObject* WorldContext);

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
	TMap<TObjectPtr<UClass>, FSBActorPoolList> FreeActors;

	TMap<UClass*, int32> ActiveCounts;
	int32 TotalSpawned = 0;
};
