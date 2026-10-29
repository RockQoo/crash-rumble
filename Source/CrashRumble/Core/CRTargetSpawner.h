#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRTargetSpawner.generated.h"

class ACRCharacter;
class UCRTuningSettings;
enum class ECRTeam : uint8;

/** 팀 전투 설정. 팀 전투 탭 값으로 만들거나 벤치마크가 직접 채운다. */
struct FCRBattleSetup
{
	int32 PerTeam = 10;
	int32 SwordWeight = 4;
	int32 LaserWeight = 3;
	int32 ProjectileWeight = 3;
	int32 MissileWeight = 0;
	float TeamDistance = 700.f;
	float Spread = 350.f;
	/** 사거리 안에서 옆걸음 세기 (0 = 서서 쏨). */
	float Strafe = 0.f;
	bool bSpectate = true;
	/** 진영 끝에 가로줄로 서서(여러 줄) 앞으로 전진, 교전 거리 안의 적과만 싸운다. 끄면 진영 가운데 둥글게 모여 가장 가까운 적에게 달려든다. */
	bool bLineFormation = false;
	float FrontWidth = 6000.f;
	int32 PerRow = 25;
	float RowSpacing = 250.f;
	float EngageRange = 1300.f;
	/** 전투원끼리 주는 피해 배율. */
	float DamageScale = 1.f;

	static FCRBattleSetup FromTuning(const UCRTuningSettings& Tuning, int32 InPerTeam);
};

/**
 * 다수 타격 테스트용 몬스터 생성기. 몬스터는 UCRActorPoolSubsystem에서 꺼내고 반납한다.
 * 죽거나 맵 밖으로 떨어진 몬스터는 하늘에서 다시 떨어진다 (일반: 플레이어 주변, 팀 전투: 자기 진영).
 */
UCLASS()
class CRASHRUMBLE_API UCRTargetSpawnerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRTargetSpawnerSubsystem* Get(const UObject* WorldContext);

	void SpawnTargets(int32 Count);
	void ClearTargets();
	int32 GetActiveCount() const { return ActiveTargets.Num(); }

	/** 기존 몬스터를 정리하고 플레이어 양옆에 레드/블루 진영을 PerTeam 마리씩 세운다. */
	void StartBattle(int32 PerTeam);
	void StartBattle(const FCRBattleSetup& Setup);
	bool IsBattleActive() const { return bBattleActive; }
	const FCRBattleSetup& GetBattleSetup() const { return BattleSetup; }
	/** 이 팀이 전진하는 방향 (상대 진영 쪽). */
	FVector GetTeamForward(ECRTeam Team) const;
	/** 이 팀 진영(출발선)에서 전진 방향으로 얼마나 나왔는지. 상대 출발선 = TeamDistance * 2. */
	float GetAdvance(ECRTeam Team, const FVector& Location) const;
	ACRCharacter* FindNearestEnemy(const ACRCharacter* Self) const;
	/** 팀 상관없이 살아 있는 가장 가까운 몬스터. */
	ACRCharacter* FindNearestTarget(const FVector& From) const;
	void NotifyTeamMemberDied(ECRTeam Team);
	int32 GetAliveCount(ECRTeam Team) const;
	int32 GetKills(ECRTeam Team) const;

	/** 플레이어 주변 생성 반경 안의 바닥 위 지점 (+Height). 바닥을 못 찾으면 false. */
	bool FindSpawnLocation(float Height, FVector& OutLocation) const;
	/** 팀 전투 중이면 자기 진영, 아니면 플레이어 주변. */
	bool FindRespawnLocation(ECRTeam Team, float Height, FVector& OutLocation) const;

private:
	bool FindGroundAround(const FVector& Center, float MinRadius, float MaxRadius, float Height, FVector& OutLocation) const;
	void SetPlayerSpectating(bool bSpectating);

	UPROPERTY()
	TArray<TObjectPtr<ACRCharacter>> ActiveTargets;

	bool bBattleActive = false;
	FCRBattleSetup BattleSetup;
	bool bPlayerSpectating = false;
	FVector BattleCenter = FVector::ZeroVector;
	FVector BattleAxis = FVector::RightVector; // 레드 → 블루
	int32 RedKills = 0;
	int32 BlueKills = 0;
};
