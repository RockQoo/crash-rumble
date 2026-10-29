#include "Core/CRTargetSpawner.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Character/CRCharacter.h"
#include "Core/CRActorPool.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
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

UCRTargetSpawnerSubsystem* UCRTargetSpawnerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRTargetSpawnerSubsystem>() : nullptr;
}

bool UCRTargetSpawnerSubsystem::FindGroundAround(const FVector& Center, float MinRadius, float MaxRadius, float Height,
	FVector& OutLocation) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRSpawnGround), false);
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

bool UCRTargetSpawnerSubsystem::FindSpawnLocation(float Height, FVector& OutLocation) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const float Radius = FMath::Max(UCRTuningSubsystem::GetSettings(this).SpawnRadius, MinSpawnDistance + 50.f);
	return FindGroundAround(Player->GetActorLocation(), MinSpawnDistance, Radius, Height, OutLocation);
}

bool UCRTargetSpawnerSubsystem::FindRespawnLocation(ECRTeam Team, float Height, FVector& OutLocation) const
{
	if (!bBattleActive || Team == ECRTeam::None)
	{
		return FindSpawnLocation(Height, OutLocation);
	}
	const float Side = Team == ECRTeam::Red ? -1.f : 1.f;
	const FVector Base = BattleCenter + BattleAxis * Side * BattleSetup.TeamDistance;
	if (BattleSetup.bLineFormation)
	{
		// 자기 진영 뒷줄 아무 데서나 (전열 폭 안, 줄 깊이 안)
		const FVector Across = FVector::CrossProduct(FVector::UpVector, BattleAxis);
		const int32 Rows = FMath::DivideAndRoundUp(FMath::Max(BattleSetup.PerTeam, 1), FMath::Max(BattleSetup.PerRow, 1));
		const FVector Point = Base + Across * FMath::FRandRange(-0.5f, 0.5f) * BattleSetup.FrontWidth
			- BattleAxis * Side * FMath::FRandRange(0.f, Rows * BattleSetup.RowSpacing);
		return FindGroundAround(Point, 0.f, BattleSetup.RowSpacing * 0.5f, Height, OutLocation);
	}
	return FindGroundAround(Base, 0.f, BattleSetup.Spread, Height, OutLocation);
}

FVector UCRTargetSpawnerSubsystem::GetTeamForward(ECRTeam Team) const
{
	return Team == ECRTeam::Red ? BattleAxis : -BattleAxis;
}

float UCRTargetSpawnerSubsystem::GetAdvance(ECRTeam Team, const FVector& Location) const
{
	const float Side = Team == ECRTeam::Red ? -1.f : 1.f;
	const FVector Start = BattleCenter + BattleAxis * Side * BattleSetup.TeamDistance;
	return FVector::DotProduct(Location - Start, GetTeamForward(Team));
}

void UCRTargetSpawnerSubsystem::SpawnTargets(int32 Count)
{
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
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
			UE_LOG(LogTemp, Warning, TEXT("CRSpawn: no ground found"));
			continue;
		}
		const FRotator Facing(0.f, (Player->GetActorLocation() - Location).Rotation().Yaw, 0.f);
		if (ACRCharacter* Target = Pool->Acquire<ACRCharacter>(FTransform(Facing, Location)))
		{
			Target->SetupAsMonster(MonsterAccent);
			ActiveTargets.Add(Target);
		}
	}
}

FCRBattleSetup FCRBattleSetup::FromTuning(const UCRTuningSettings& Tuning, int32 InPerTeam)
{
	FCRBattleSetup Setup;
	Setup.PerTeam = InPerTeam;
	Setup.SwordWeight = Tuning.BattleSwordRatio;
	Setup.LaserWeight = Tuning.BattleLaserRatio;
	Setup.ProjectileWeight = Tuning.BattleProjectileRatio;
	Setup.MissileWeight = Tuning.BattleMissileRatio;
	Setup.TeamDistance = Tuning.BattleTeamDistance;
	Setup.Spread = Tuning.BattleSpread;
	Setup.Strafe = Tuning.BattleStrafe;
	Setup.bSpectate = Tuning.bBattleSpectate;
	return Setup;
}

void UCRTargetSpawnerSubsystem::StartBattle(int32 PerTeam)
{
	StartBattle(FCRBattleSetup::FromTuning(UCRTuningSubsystem::GetSettings(this), PerTeam));
}

void UCRTargetSpawnerSubsystem::StartBattle(const FCRBattleSetup& Setup)
{
	ClearTargets();
	BattleSetup = Setup;
	const int32 PerTeam = Setup.PerTeam;

	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
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

	for (const ECRTeam Team : { ECRTeam::Red, ECRTeam::Blue })
	{
		const float Side = Team == ECRTeam::Red ? -1.f : 1.f;
		const FVector Base = BattleCenter + BattleAxis * Side * Setup.TeamDistance;
		const FRotator Facing(0.f, (-BattleAxis * Side).Rotation().Yaw, 0.f);
		const int32 SwordWeight = FMath::Max(Setup.SwordWeight, 0);
		const int32 LaserWeight = FMath::Max(Setup.LaserWeight, 0);
		const int32 ProjectileWeight = FMath::Max(Setup.ProjectileWeight, 0);
		const int32 TotalWeight = FMath::Max(SwordWeight + LaserWeight + ProjectileWeight + FMath::Max(Setup.MissileWeight, 0), 1);
		for (int32 i = 0; i < PerTeam; ++i)
		{
			// 비율대로 고르게 섞는다 (예: 4:3:3 → 10명 중 칼 4, 레이저 3, 투사체 3)
			const int32 Slot = FMath::FloorToInt((i + 0.5f) * TotalWeight / FMath::Max(PerTeam, 1)) % TotalWeight;
			const ECRWeaponType Weapon = Slot < SwordWeight ? ECRWeaponType::Sword
				: Slot < SwordWeight + LaserWeight ? ECRWeaponType::Laser
				: Slot < SwordWeight + LaserWeight + ProjectileWeight ? ECRWeaponType::Projectile
				: ECRWeaponType::Missile;
			FVector Location;
			bool bFound = false;
			if (Setup.bLineFormation)
			{
				// 가로줄: 맨 뒷줄(맵 끝)부터 PerRow명씩, 줄은 가운데 쪽으로 RowSpacing씩
				const int32 PerRow = FMath::Max(Setup.PerRow, 1);
				const int32 Row = i / PerRow;
				const int32 Column = i % PerRow;
				const FVector Across = FVector::CrossProduct(FVector::UpVector, BattleAxis);
				const float Lateral = ((Column + 0.5f) / PerRow - 0.5f) * Setup.FrontWidth;
				const FVector Point = Base + Across * Lateral - BattleAxis * Side * Row * Setup.RowSpacing;
				bFound = FindGroundAround(Point, 0.f, 20.f, 0.f, Location);
			}
			else
			{
				bFound = FindGroundAround(Base, 0.f, Setup.Spread, 0.f, Location);
			}
			if (!bFound)
			{
				continue;
			}
			if (ACRCharacter* Fighter = Pool->Acquire<ACRCharacter>(FTransform(Facing, Location)))
			{
				Fighter->SetupAsMonster(Team == ECRTeam::Red ? RedAccent : BlueAccent, Team, Weapon);
				ActiveTargets.Add(Fighter);
			}
		}
	}

	SetPlayerSpectating(Setup.bSpectate);
}

void UCRTargetSpawnerSubsystem::ClearTargets()
{
	if (UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this))
	{
		for (ACRCharacter* Target : ActiveTargets)
		{
			Pool->Release(Target);
		}
	}
	ActiveTargets.Reset();
	bBattleActive = false;
	SetPlayerSpectating(false);
}

void UCRTargetSpawnerSubsystem::SetPlayerSpectating(bool bSpectating)
{
	if (bPlayerSpectating == bSpectating)
	{
		return;
	}
	bPlayerSpectating = bSpectating;
	if (ACRCharacter* Player = Cast<ACRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->SetSpectating(bSpectating);
	}
}

ACRCharacter* UCRTargetSpawnerSubsystem::FindNearestEnemy(const ACRCharacter* Self) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_FindNearestEnemy);
	if (!Self || Self->GetTeam() == ECRTeam::None)
	{
		return nullptr;
	}
	const FVector From = Self->GetActorLocation();
	ACRCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (ACRCharacter* Other : ActiveTargets)
	{
		if (!Other || Other->GetTeam() == ECRTeam::None || Other->GetTeam() == Self->GetTeam() || Other->IsDead() || Other->IsInPool())
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

ACRCharacter* UCRTargetSpawnerSubsystem::FindNearestTarget(const FVector& From) const
{
	ACRCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (ACRCharacter* Other : ActiveTargets)
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

void UCRTargetSpawnerSubsystem::NotifyTeamMemberDied(ECRTeam Team)
{
	// 죽은 쪽의 상대 팀 처치 수
	if (Team == ECRTeam::Red)
	{
		++BlueKills;
	}
	else if (Team == ECRTeam::Blue)
	{
		++RedKills;
	}
}

int32 UCRTargetSpawnerSubsystem::GetAliveCount(ECRTeam Team) const
{
	int32 Count = 0;
	for (const ACRCharacter* Target : ActiveTargets)
	{
		Count += Target && Target->GetTeam() == Team && !Target->IsDead() ? 1 : 0;
	}
	return Count;
}

int32 UCRTargetSpawnerSubsystem::GetKills(ECRTeam Team) const
{
	return Team == ECRTeam::Red ? RedKills : Team == ECRTeam::Blue ? BlueKills : 0;
}
