#include "Core/SBTargetSpawner.h"

#include "Character/SBSnowmanCharacter.h"
#include "Core/SBActorPool.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float MinSpawnDistance = 350.f;
	constexpr float SpawnCapsuleHalfHeight = 100.f;
	constexpr int32 MaxSpawnTries = 8;
	const FLinearColor MonsterAccent(0.9f, 0.1f, 0.6f);
}

USBTargetSpawnerSubsystem* USBTargetSpawnerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<USBTargetSpawnerSubsystem>() : nullptr;
}

bool USBTargetSpawnerSubsystem::FindSpawnLocation(float Height, FVector& OutLocation) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	const float Radius = FMath::Max(USBTuningSubsystem::GetSettings(this).SpawnRadius, MinSpawnDistance + 50.f);
	const FVector Center = Player->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBSpawnGround), false);
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (int32 Try = 0; Try < MaxSpawnTries; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		const float Distance = FMath::Sqrt(FMath::FRandRange(FMath::Square(MinSpawnDistance), FMath::Square(Radius)));
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
}
