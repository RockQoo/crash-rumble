#include "Core/CRLootSubsystem.h"

#include "Character/CRCharacter.h"
#include "Character/CRLoadoutComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/CRFxSubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr int32 MaxCoins = 512;
	constexpr float CoinGravity = 1800.f;
	constexpr float CoinBounce = 0.45f;
	constexpr float CoinRadius = 9.f;
	constexpr float PickupDistance = 70.f;
	constexpr float MagnetAccel = 5000.f;
	constexpr float BlinkFrom = 0.8f; // 수명 80%부터 깜빡이며 사라질 때를 알림
	const FLinearColor CoinColor(1.6f, 1.05f, 0.1f);
	const FTransform HiddenTransform(FQuat::Identity, FVector(0.f, 0.f, -100000.f), FVector::ZeroVector);
}

UCRLootSubsystem* UCRLootSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRLootSubsystem>() : nullptr;
}

TStatId UCRLootSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRLootSubsystem, STATGROUP_Tickables);
}

bool UCRLootSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCRLootSubsystem::Deinitialize()
{
	CoinActor = nullptr;
	CoinMesh = nullptr;
	Super::Deinitialize();
}

bool UCRLootSubsystem::EnsureComponent()
{
	if (CoinMesh)
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	CoinActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	USceneComponent* Root = NewObject<USceneComponent>(CoinActor, TEXT("Root"));
	CoinActor->SetRootComponent(Root);
	Root->RegisterComponent();

	CoinMesh = NewObject<UInstancedStaticMeshComponent>(CoinActor);
	CoinMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	CoinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoinMesh->SetCastShadow(false);
	CoinMesh->SetCanEverAffectNavigation(false);
	CoinMesh->SetMobility(EComponentMobility::Movable);
	CoinMesh->SetNumCustomDataFloats(CRMaterialData::InstanceFloats);
	CoinMesh->SetupAttachment(Root);
	CoinMesh->RegisterComponent();
	if (UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial())
	{
		CoinMesh->SetMaterial(0, Toon);
	}
	Coins.SetNum(MaxCoins);
	return true;
}

void UCRLootSubsystem::DropForDeath(const ACRCharacter* Victim)
{
	if (!Victim || Victim->IsDoppel() || Victim->IsPlayerControlled())
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const ECRTeam Team = Victim->GetTeam();
	const bool bWaveMonster = Team == ECRTeam::Monster;
	const bool bLooseMonster = Victim->IsMonster() && Team == ECRTeam::None && Victim->GetTrainingRole() == ECRTrainingRole::None;
	const bool bDummy = Victim->GetTrainingRole() == ECRTrainingRole::Dummy || Victim->GetTrainingRole() == ECRTrainingRole::MovingTarget;
	if (!bWaveMonster && !bLooseMonster && !(bDummy && Tuning.bTrainingGold))
	{
		return;
	}
	const int32 Count = FMath::RandRange(FMath::Min(Tuning.CoinMin, Tuning.CoinMax), FMath::Max(Tuning.CoinMin, Tuning.CoinMax));
	SpawnCoins(Victim->GetActorLocation(), Count, Tuning.CoinValue);
}

void UCRLootSubsystem::SpawnCoins(const FVector& Location, int32 Count, int32 ValueEach)
{
	if (Count <= 0 || !EnsureComponent())
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	float GroundZ = Location.Z - 100.f;
	FHitResult Hit;
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0.f, 0.f, 2000.f), GroundTypes,
		FCollisionQueryParams(SCENE_QUERY_STAT(CRCoinGround), false)))
	{
		GroundZ = Hit.ImpactPoint.Z;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		// 고정 버퍼를 돌려 쓴다: 꽉 차면 가장 오래된 동전을 덮어씀
		FCoin& Coin = Coins[NextCoin];
		NextCoin = (NextCoin + 1) % MaxCoins;
		if (!Coin.bAlive)
		{
			++AliveCoins;
		}
		Coin = FCoin();
		Coin.bAlive = true;
		Coin.Location = Location + FVector(0.f, 0.f, 20.f);
		const FVector2D Side = FMath::RandPointInCircle(1.f);
		Coin.Velocity = FVector(Side.X, Side.Y, 0.f) * Tuning.CoinScatter + FVector(0.f, 0.f, FMath::FRandRange(450.f, 700.f));
		Coin.GroundZ = GroundZ;
		Coin.Spin = FMath::FRandRange(0.f, 360.f);
		Coin.Value = FMath::Max(ValueEach, 1);
	}
}

void UCRLootSubsystem::ClearCoins()
{
	for (FCoin& Coin : Coins)
	{
		Coin.bAlive = false;
	}
	AliveCoins = 0;
}

void UCRLootSubsystem::Collect(FCoin& Coin, ACRCharacter* Collector)
{
	// 도플갱어가 주워도 주인 골드
	ACRCharacter* Owner = Collector->GetHero() ? Collector->GetHero() : Collector;
	if (UCRLoadoutComponent* Loadout = Owner->GetLoadout())
	{
		Loadout->AddGold(Coin.Value);
	}
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this); Fx && UCRTuningSubsystem::GetSettings(this).bSparkle)
	{
		Fx->SpawnSparkle(Coin.Location, 14.f);
	}
	Coin.bAlive = false;
	--AliveCoins;
}

void UCRLootSubsystem::Tick(float DeltaTime)
{
	if (AliveCoins <= 0 && LastDrawn == 0)
	{
		return;
	}
	if (!EnsureComponent())
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 줍는 쪽: 플레이어와 그 도플갱어
	TArray<ACRCharacter*, TInlineAllocator<2>> Collectors;
	if (ACRCharacter* Player = Cast<ACRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (!Player->IsDead() && !Player->IsHidden())
		{
			Collectors.Add(Player);
		}
		if (ACRCharacter* Doppel = Player->GetDoppel(); Doppel && !Doppel->IsDead())
		{
			Collectors.Add(Doppel);
		}
	}

	TArray<FTransform> Transforms;
	Transforms.Reserve(AliveCoins);
	for (FCoin& Coin : Coins)
	{
		if (!Coin.bAlive)
		{
			continue;
		}
		Coin.Age += DeltaTime;
		if (Coin.Age > Tuning.CoinLifetime)
		{
			Coin.bAlive = false;
			--AliveCoins;
			continue;
		}

		// 자석: 가장 가까운 줍는 쪽이 반경 안이면 점점 빠르게 빨려간다 (한 번 붙으면 계속)
		ACRCharacter* Nearest = nullptr;
		float NearestDistSq = FMath::Square(Tuning.CoinMagnetRadius);
		for (ACRCharacter* Collector : Collectors)
		{
			const float DistSq = FVector::DistSquared(Collector->GetActorLocation(), Coin.Location);
			if (DistSq < NearestDistSq || (Coin.bMagnet && !Nearest))
			{
				NearestDistSq = DistSq;
				Nearest = Collector;
			}
		}
		if (Nearest && Coin.Age > 0.25f)
		{
			Coin.bMagnet = true;
			const FVector To = Nearest->GetActorLocation() - Coin.Location;
			if (To.SizeSquared() < FMath::Square(PickupDistance))
			{
				Collect(Coin, Nearest);
				continue;
			}
			Coin.Velocity += To.GetSafeNormal() * MagnetAccel * DeltaTime;
			Coin.Velocity = Coin.Velocity.GetClampedToMaxSize(Tuning.CoinMagnetSpeed);
			Coin.Location += Coin.Velocity * DeltaTime;
		}
		else
		{
			// 튕기며 흩어졌다가 바닥에 멈춘다
			Coin.Velocity.Z -= CoinGravity * DeltaTime;
			Coin.Location += Coin.Velocity * DeltaTime;
			if (Coin.Location.Z < Coin.GroundZ + CoinRadius)
			{
				Coin.Location.Z = Coin.GroundZ + CoinRadius;
				Coin.Velocity.Z = FMath::Abs(Coin.Velocity.Z) * CoinBounce;
				Coin.Velocity.X *= 0.6f;
				Coin.Velocity.Y *= 0.6f;
				if (Coin.Velocity.Z < 60.f)
				{
					Coin.Velocity.Z = 0.f;
				}
			}
		}
		Coin.Spin = FMath::Fmod(Coin.Spin + DeltaTime * 360.f, 360.f);

		// 수명 끝무렵 깜빡임
		const bool bBlinkOff = Coin.Age > Tuning.CoinLifetime * BlinkFrom && FMath::Fmod(Coin.Age, 0.2f) < 0.08f;
		if (!bBlinkOff)
		{
			// 세워서 도는 납작한 원기둥 (지름 22cm)
			const FQuat Rotation = FRotator(0.f, Coin.Spin, 90.f).Quaternion();
			Transforms.Add(FTransform(Rotation, Coin.Location, FVector(0.22f, 0.22f, 0.04f)));
		}
	}

	// 인스턴스 갱신: 모자라면 늘리고, 남는 건 숨긴다
	const int32 Need = Transforms.Num();
	const int32 Have = CoinMesh->GetInstanceCount();
	if (Need > Have)
	{
		TArray<FTransform> Extra;
		Extra.Init(HiddenTransform, FMath::Max(Need - Have, 32));
		CoinMesh->AddInstances(Extra, false, true);
		for (int32 i = Have; i < CoinMesh->GetInstanceCount(); ++i)
		{
			CoinMesh->SetCustomData(i, { CoinColor.R, CoinColor.G, CoinColor.B, 1.f }, false);
		}
	}
	if (Need > 0)
	{
		CoinMesh->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
	}
	for (int32 i = Need; i < LastDrawn; ++i)
	{
		CoinMesh->UpdateInstanceTransform(i, HiddenTransform, true, false, true);
	}
	LastDrawn = Need;
	CoinMesh->UpdateBounds();
	CoinMesh->MarkRenderStateDirty();
}
