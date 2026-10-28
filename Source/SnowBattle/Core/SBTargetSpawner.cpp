#include "Core/SBTargetSpawner.h"

#include "Character/SBSnowmanCharacter.h"
#include "Core/SBActorPool.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float MinSpawnDistance = 350.f;
	constexpr float SpawnCapsuleHalfHeight = 100.f;
	constexpr int32 MaxSpawnTries = 8;
	const FLinearColor MonsterAccent(0.9f, 0.1f, 0.6f);
	const FLinearColor RedAccent(1.f, 0.08f, 0.05f);
	const FLinearColor BlueAccent(0.05f, 0.3f, 1.f);
}

USBTargetSpawnerSubsystem* USBTargetSpawnerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<USBTargetSpawnerSubsystem>() : nullptr;
}

bool USBTargetSpawnerSubsystem::FindGroundAround(const FVector& Center, float MinRadius, float MaxRadius, float Height,
	FVector& OutLocation) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBSpawnGround), false);
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (int32 Try = 0; Try < MaxSpawnTries; ++Try)
	{
		// 원 안에 고르게 퍼지도록 반지름은 제곱 기준으로 뽑는다
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		const float Distance = FMath::Sqrt(FMath::FRandRange(FMath::Square(MinRadius), FMath::Square(MaxRadius)));
		const FVector Probe = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Probe + FVector(0, 0, 2000.f), Probe - FVector(0, 0, 5000.f),
			GroundTypes, Params))
		{
			OutLocation = Hit.ImpactPoint + FVector(0.f, 0.f, SpawnCapsuleHalfHeight + Height);
			return true;
		}
	}
	return false;
}

bool USBTargetSpawnerSubsystem::FindSpawnLocation(float Height, FVector& OutLocation) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const float Radius = FMath::Max(USBTuningSubsystem::GetSettings(this).SpawnRadius, MinSpawnDistance + 50.f);
	return FindGroundAround(Player->GetActorLocation(), MinSpawnDistance, Radius, Height, OutLocation);
}

bool USBTargetSpawnerSubsystem::FindRespawnLocation(ESBTeam Team, float Height, FVector& OutLocation) const
{
	if (!bBattleActive || Team == ESBTeam::None)
	{
		return FindSpawnLocation(Height, OutLocation);
	}
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	const float Side = Team == ESBTeam::Red ? -1.f : 1.f;
	return FindGroundAround(BattleCenter + BattleAxis * Side * Tuning.BattleTeamDistance, 0.f, Tuning.BattleSpread, Height, OutLocation);
}

void USBTargetSpawnerSubsystem::SpawnTargets(int32 Count)
{
	USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pool || !Player)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Location;
		if (!FindSpawnLocation(0.f, Location))
		{
			UE_LOG(LogTemp, Warning, TEXT("SBSpawn: no ground found"));
			continue;
		}
		const FRotator Facing(0.f, (Player->GetActorLocation() - Location).Rotation().Yaw, 0.f);
		if (ASBSnowmanCharacter* Target = Pool->Acquire<ASBSnowmanCharacter>(FTransform(Facing, Location)))
		{
			Target->SetupAsMonster(MonsterAccent);
			ActiveTargets.Add(Target);
		}
	}
}

void USBTargetSpawnerSubsystem::StartBattle(int32 PerTeam)
{
	ClearTargets();

	USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pool || !Player)
	{
		return;
	}

	// 화면 기준 왼쪽 레드, 오른쪽 블루
	BattleCenter = Player->GetActorLocation();
	const FRotator ViewYaw(0.f, Player->GetControlRotation().Yaw, 0.f);
	BattleAxis = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y);
	bBattleActive = true;
	RedKills = 0;
	BlueKills = 0;

	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	for (const ESBTeam Team : { ESBTeam::Red, ESBTeam::Blue })
	{
		const float Side = Team == ESBTeam::Red ? -1.f : 1.f;
		const FVector Base = BattleCenter + BattleAxis * Side * Tuning.BattleTeamDistance;
		const FRotator Facing(0.f, (-BattleAxis * Side).Rotation().Yaw, 0.f);
		const int32 SwordWeight = FMath::Max(Tuning.BattleSwordRatio, 0);
		const int32 LaserWeight = FMath::Max(Tuning.BattleLaserRatio, 0);
		const int32 TotalWeight = FMath::Max(SwordWeight + LaserWeight + FMath::Max(Tuning.BattleProjectileRatio, 0), 1);
		for (int32 i = 0; i < PerTeam; ++i)
		{
			// 비율대로 고르게 섞는다 (예: 4:3:3 → 10명 중 칼 4, 레이저 3, 투사체 3)
			const int32 Slot = FMath::FloorToInt((i + 0.5f) * TotalWeight / FMath::Max(PerTeam, 1)) % TotalWeight;
			const ESBWeaponType Weapon = Slot < SwordWeight ? ESBWeaponType::Sword
				: Slot < SwordWeight + LaserWeight ? ESBWeaponType::Laser
				: ESBWeaponType::Projectile;
			FVector Location;
			if (!FindGroundAround(Base, 0.f, Tuning.BattleSpread, 0.f, Location))
			{
				continue;
			}
			if (ASBSnowmanCharacter* Fighter = Pool->Acquire<ASBSnowmanCharacter>(FTransform(Facing, Location)))
			{
				Fighter->SetupAsMonster(Team == ESBTeam::Red ? RedAccent : BlueAccent, Team, Weapon);
				ActiveTargets.Add(Fighter);
			}
		}
	}

	SetPlayerSpectating(Tuning.bBattleSpectate);
}

void USBTargetSpawnerSubsystem::ClearTargets()
{
	if (USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this))
	{
		for (ASBSnowmanCharacter* Target : ActiveTargets)
		{
			Pool->Release(Target);
		}
	}
	ActiveTargets.Reset();
	bBattleActive = false;
	SetPlayerSpectating(false);
}

void USBTargetSpawnerSubsystem::SetPlayerSpectating(bool bSpectating)
{
	if (bPlayerSpectating == bSpectating)
	{
		return;
	}
	bPlayerSpectating = bSpectating;
	if (ASBSnowmanCharacter* Player = Cast<ASBSnowmanCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->SetSpectating(bSpectating);
	}
}

ASBSnowmanCharacter* USBTargetSpawnerSubsystem::FindNearestEnemy(const ASBSnowmanCharacter* Self) const
{
	if (!Self || Self->GetTeam() == ESBTeam::None)
	{
		return nullptr;
	}
	const FVector From = Self->GetActorLocation();
	ASBSnowmanCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (ASBSnowmanCharacter* Other : ActiveTargets)
	{
		if (!Other || Other->GetTeam() == ESBTeam::None || Other->GetTeam() == Self->GetTeam() || Other->IsDead() || Other->IsInPool())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(From, Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

ASBSnowmanCharacter* USBTargetSpawnerSubsystem::FindNearestTarget(const FVector& From) const
{
	ASBSnowmanCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (ASBSnowmanCharacter* Other : ActiveTargets)
	{
		if (!Other || Other->IsDead() || Other->IsInPool())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(From, Other->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

void USBTargetSpawnerSubsystem::NotifyTeamMemberDied(ESBTeam Team)
{
	// 죽은 쪽의 상대 팀 처치 수
	if (Team == ESBTeam::Red)
	{
		++BlueKills;
	}
	else if (Team == ESBTeam::Blue)
	{
		++RedKills;
	}
}

int32 USBTargetSpawnerSubsystem::GetAliveCount(ESBTeam Team) const
{
	int32 Count = 0;
	for (const ASBSnowmanCharacter* Target : ActiveTargets)
	{
		Count += Target && Target->GetTeam() == Team && !Target->IsDead() ? 1 : 0;
	}
	return Count;
}

int32 USBTargetSpawnerSubsystem::GetKills(ESBTeam Team) const
{
	return Team == ESBTeam::Red ? RedKills : Team == ESBTeam::Blue ? BlueKills : 0;
}
