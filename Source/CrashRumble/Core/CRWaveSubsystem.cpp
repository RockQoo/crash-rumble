#include "Core/CRWaveSubsystem.h"

#include "Character/CRCharacter.h"
#include "Character/CRLoadoutComponent.h"
#include "Core/CRActorPool.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 MaxWaveSpawnTries = 6;
	const FVector North(1.f, 0.f, 0.f);
	const FLinearColor SwordAccent(0.95f, 0.12f, 0.1f);
	const FLinearColor GunAccent(1.f, 0.55f, 0.05f);
	const FLinearColor LauncherAccent(0.55f, 0.15f, 0.95f);
}

UCRWaveSubsystem* UCRWaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRWaveSubsystem>() : nullptr;
}

TStatId UCRWaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRWaveSubsystem, STATGROUP_Tickables);
}

bool UCRWaveSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCRWaveSubsystem::StartRun(bool bInGameRun)
{
	StopRun();
	bRunning = true;
	bGameRun = bInGameRun;
	bGameOver = false;
	Elapsed = 0.f;
	SpawnTimer = 1.f; // 시작하고 1초 뒤 첫 무리
	Kills = 0;
	FinalGold = 0;
}

void UCRWaveSubsystem::StopRun()
{
	if (UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this))
	{
		for (ACRCharacter* Monster : Monsters)
		{
			if (Monster && !Monster->IsInPool())
			{
				Pool->Release(Monster);
			}
		}
	}
	Monsters.Reset();
	Corpses.Reset();
	bRunning = false;
}

int32 UCRWaveSubsystem::GetAliveCount() const
{
	int32 Count = 0;
	for (const ACRCharacter* Monster : Monsters)
	{
		Count += Monster && !Monster->IsDead() ? 1 : 0;
	}
	return Count;
}

void UCRWaveSubsystem::NotifyMonsterDied(ACRCharacter* Monster)
{
	if (!Monsters.Contains(Monster))
	{
		return;
	}
	++Kills;
	Corpses.Emplace(Monster, GetWorld()->GetTimeSeconds() + UCRTuningSubsystem::GetSettings(this).WaveCorpseTime);
}

void UCRWaveSubsystem::NotifyPlayerDied()
{
	if (!IsGameRun() || bGameOver)
	{
		return;
	}
	bGameOver = true;
	const ACRCharacter* Player = Cast<ACRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	FinalGold = Player && Player->GetLoadout() ? Player->GetLoadout()->GetGold() : 0;
}

void UCRWaveSubsystem::ReleaseMonster(ACRCharacter* Monster)
{
	Monsters.Remove(Monster);
	if (UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this); Pool && Monster && !Monster->IsInPool())
	{
		Pool->Release(Monster);
	}
}

void UCRWaveSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	// 쓰러진 몬스터 반납
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 i = Corpses.Num() - 1; i >= 0; --i)
	{
		if (Now >= Corpses[i].Value)
		{
			ReleaseMonster(Corpses[i].Key.Get());
			Corpses.RemoveAtSwap(i);
		}
	}
	if (bGameOver)
	{
		return;
	}

	Elapsed += DeltaTime;
	SpawnTimer -= DeltaTime;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer += FMath::Max(UCRTuningSubsystem::GetSettings(this).WaveInterval, 0.2f);
		SpawnBatch();
	}
}

void UCRWaveSubsystem::SpawnBatch()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pool || !Player)
	{
		return;
	}
	// 시간이 지날수록 한 번에 더 많이 (WaveRampSeconds마다 +1)
	const int32 Ramp = FMath::FloorToInt(Elapsed / FMath::Max(Tuning.WaveRampSeconds, 1.f));
	const int32 MinCount = FMath::Max(Tuning.WaveMinCount, 1) + Ramp;
	const int32 MaxCount = FMath::Max(Tuning.WaveMaxCount, Tuning.WaveMinCount) + Ramp;
	int32 Count = FMath::RandRange(MinCount, MaxCount);
	Count = FMath::Min(Count, FMath::Max(Tuning.WaveMaxAlive - GetAliveCount(), 0));

	const int32 SwordWeight = FMath::Max(Tuning.WaveSwordRatio, 0);
	const int32 GunWeight = FMath::Max(Tuning.WaveGunRatio, 0);
	const int32 TotalWeight = FMath::Max(SwordWeight + GunWeight + FMath::Max(Tuning.WaveLauncherRatio, 0), 1);
	// 보폭이 합과 서로소여야 모든 칸을 돈다 (합이 7의 배수일 때 보폭 7이면 한 무기만 나왔다)
	int32 Stride = 7;
	while (TotalWeight > 1 && FMath::GreatestCommonDivisor(Stride, TotalWeight) != 1)
	{
		++Stride;
	}
	const FVector Across = FVector::CrossProduct(FVector::UpVector, North);
	const FCollisionObjectQueryParams GroundTypes = []()
	{
		FCollisionObjectQueryParams Types;
		Types.AddObjectTypesToQuery(ECC_WorldStatic);
		Types.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Types;
	}();

	for (int32 i = 0; i < Count; ++i)
	{
		// 비율대로 고르게 섞는다 (5:3:2 → 10마리 중 칼 5, 총 3, 런처 2)
		const int32 Slot = (SpawnSerial++ * Stride) % TotalWeight;
		const ECRWeaponType Weapon = Slot < SwordWeight ? ECRWeaponType::Sword
			: Slot < SwordWeight + GunWeight ? ECRWeaponType::Projectile : ECRWeaponType::Missile;
		const FLinearColor Accent = Weapon == ECRWeaponType::Sword ? SwordAccent : Weapon == ECRWeaponType::Projectile ? GunAccent : LauncherAccent;

		// 플레이어 북쪽 화면 밖 가로줄 아무 데서나, 바닥 위에 (구덩이면 다시 뽑기)
		bool bFound = false;
		FVector Location;
		for (int32 Try = 0; Try < MaxWaveSpawnTries && !bFound; ++Try)
		{
			const FVector Probe = Player->GetActorLocation() + North * (Tuning.WaveSpawnDistance + FMath::FRandRange(0.f, 300.f))
				+ Across * FMath::FRandRange(-0.5f, 0.5f) * Tuning.WaveSpawnWidth;
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, Probe + FVector(0.f, 0.f, 2000.f), Probe - FVector(0.f, 0.f, 5000.f), GroundTypes,
				FCollisionQueryParams(SCENE_QUERY_STAT(CRWaveGround), false)))
			{
				Location = Hit.ImpactPoint + FVector(0.f, 0.f, 110.f);
				bFound = true;
			}
		}
		if (!bFound)
		{
			continue;
		}
		const FRotator Facing(0.f, (-North).Rotation().Yaw, 0.f);
		if (ACRCharacter* Monster = Pool->Acquire<ACRCharacter>(FTransform(Facing, Location)))
		{
			Monster->SetupAsMonster(Accent, ECRTeam::Monster, Weapon);
			Monsters.Add(Monster);
		}
	}
}
