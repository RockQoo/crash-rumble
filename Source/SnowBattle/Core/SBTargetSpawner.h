#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SBTargetSpawner.generated.h"

class ASBSnowmanCharacter;

/**
 * 다수 타격 테스트용 몬스터 생성기. 몬스터는 USBActorPoolSubsystem에서 꺼내고 반납한다.
 * 죽거나 맵 밖으로 떨어진 몬스터는 플레이어 주변 하늘에서 다시 떨어진다.
 */
UCLASS()
class SNOWBATTLE_API USBTargetSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static USBTargetSpawnerSubsystem* Get(const UObject* WorldContext);

	void SpawnTargets(int32 Count);
	void ClearTargets();
	int32 GetActiveCount() const { return ActiveTargets.Num(); }

	/** 플레이어 주변 생성 반경 안의 바닥 위 지점 (+Height). 바닥을 못 찾으면 false. */
	bool FindSpawnLocation(float Height, FVector& OutLocation) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<ASBSnowmanCharacter>> ActiveTargets;
};
