#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRWaveSubsystem.generated.h"

class ACRCharacter;

/**
 * 인게임 웨이브: 플레이어 북쪽(+X) 화면 밖 가로줄에서 몬스터(칼 / 총 / 런처)가 계속 몰려온다.
 * 몬스터는 UCRActorPoolSubsystem에서 꺼내 쓰고, 쓰러지면 잠시 뒤 반납한다 (다시 일어나지 않음).
 * 인게임 판에서 플레이어가 쓰러지면 게임 오버 (결과 → R 재시작). 훈련장에서 켜면 플레이어는 평소처럼 부활.
 */
UCLASS()
class CRASHRUMBLE_API UCRWaveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRWaveSubsystem* Get(const UObject* WorldContext);

	/** bGameRun: 인게임 판 (플레이어가 쓰러지면 게임 오버). */
	void StartRun(bool bGameRun);
	/** 스폰을 멈추고 웨이브 몬스터를 모두 반납. */
	void StopRun();
	bool IsRunning() const { return bRunning; }
	bool IsGameRun() const { return bRunning && bGameRun; }
	bool IsGameOver() const { return bGameOver; }

	void NotifyMonsterDied(ACRCharacter* Monster);
	void NotifyPlayerDied();

	float GetElapsed() const { return Elapsed; }
	int32 GetKills() const { return Kills; }
	int32 GetAliveCount() const;
	/** 게임 오버 때 골드 (결과 화면). */
	int32 GetFinalGold() const { return FinalGold; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SpawnBatch();
	void ReleaseMonster(ACRCharacter* Monster);

	UPROPERTY()
	TArray<TObjectPtr<ACRCharacter>> Monsters;

	/** 쓰러진 몬스터 → 반납할 월드 시간. */
	TArray<TPair<TWeakObjectPtr<ACRCharacter>, double>> Corpses;

	bool bRunning = false;
	bool bGameRun = false;
	bool bGameOver = false;
	float Elapsed = 0.f;
	float SpawnTimer = 0.f;
	int32 Kills = 0;
	int32 SpawnSerial = 0;
	int32 FinalGold = 0;
};
