#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SBTargetSpawner.generated.h"

class ASBSnowmanCharacter;
enum class ESBTeam : uint8;

/**
 * 다수 타격 테스트용 몬스터 생성기. 몬스터는 USBActorPoolSubsystem에서 꺼내고 반납한다.
 * 죽거나 맵 밖으로 떨어진 몬스터는 하늘에서 다시 떨어진다 (일반: 플레이어 주변, 팀 전투: 자기 진영).
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

	/** 기존 몬스터를 정리하고 플레이어 양옆에 레드/블루 진영을 PerTeam 마리씩 세운다. */
	void StartBattle(int32 PerTeam);
	bool IsBattleActive() const { return bBattleActive; }
	ASBSnowmanCharacter* FindNearestEnemy(const ASBSnowmanCharacter* Self) const;
	/** 팀 상관없이 살아 있는 가장 가까운 몬스터. */
	ASBSnowmanCharacter* FindNearestTarget(const FVector& From) const;
	void NotifyTeamMemberDied(ESBTeam Team);
	int32 GetAliveCount(ESBTeam Team) const;
	int32 GetKills(ESBTeam Team) const;

	/** 플레이어 주변 생성 반경 안의 바닥 위 지점 (+Height). 바닥을 못 찾으면 false. */
	bool FindSpawnLocation(float Height, FVector& OutLocation) const;
	/** 팀 전투 중이면 자기 진영, 아니면 플레이어 주변. */
	bool FindRespawnLocation(ESBTeam Team, float Height, FVector& OutLocation) const;

private:
	bool FindGroundAround(const FVector& Center, float MinRadius, float MaxRadius, float Height, FVector& OutLocation) const;
	void SetPlayerSpectating(bool bSpectating);

	UPROPERTY()
	TArray<TObjectPtr<ASBSnowmanCharacter>> ActiveTargets;

	bool bBattleActive = false;
	bool bPlayerSpectating = false;
	FVector BattleCenter = FVector::ZeroVector;
	FVector BattleAxis = FVector::RightVector; // 레드 → 블루
	int32 RedKills = 0;
	int32 BlueKills = 0;
};
