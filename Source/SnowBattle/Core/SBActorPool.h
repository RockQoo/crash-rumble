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

	int32 GetTotalSpawned() const { return TotalSpawned; }

private:
	AActor* SpawnPooled(UClass* Class);

	UPROPERTY()
	TMap<TObjectPtr<UClass>, FSBActorPoolList> FreeActors;

	int32 TotalSpawned = 0;
};
