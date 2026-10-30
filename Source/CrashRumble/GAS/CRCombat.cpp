#include "GAS/CRCombat.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CRCharacter.h"
#include "Core/CRBenchmark.h"
#include "Core/CRTargetSpawner.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "GAS/CRAttributeSet.h"
#include "GAS/CRDamageEffect.h"
#include "GAS/CRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Map/CRBlockArena.h"
#include "Map/CRExplosiveBarrel.h"
#include "Engine/World.h"

bool CRCombat::ApplyWeaponHit(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit, ECRSkillForm Form)
{
	return ApplyWeaponHitFrom(Attacker, Victim, Weapon, Hit, Attacker ? Attacker->GetActorLocation() : FVector::ZeroVector, -1.f, Form);
}

int32 CRCombat::ApplyRadialHit(AActor* Attacker, ECRWeaponType Weapon, const FVector& Center, float Radius, float DamageOverride,
	ECRSkillForm Form, TArray<ACRCharacter*>* OutVictims)
{
	UWorld* World = Attacker ? Attacker->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CRRadialHit), false));
	TSet<AActor*> Done;
	int32 Count = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!Victim || Done.Contains(Victim))
		{
			continue;
		}
		Done.Add(Victim);
		const FVector Point = Victim->GetActorLocation();
		FHitResult Hit(Victim, Overlap.GetComponent(), Point, (Point - Center).GetSafeNormal());
		Hit.bBlockingHit = true;
		if (ApplyWeaponHitFrom(Attacker, Victim, Weapon, Hit, Center, DamageOverride, Form))
		{
			++Count;
			if (OutVictims)
			{
				if (ACRCharacter* Snowman = Cast<ACRCharacter>(Victim))
				{
					OutVictims->Add(Snowman);
				}
			}
		}
	}
	return Count;
}

bool CRCombat::DamageWorld(const FHitResult& Hit, int32 BlockDamage, AActor* Attacker)
{
	if (ACRExplosiveBarrel* Barrel = Cast<ACRExplosiveBarrel>(Hit.GetActor()))
	{
		Barrel->ApplyHit(1, Attacker);
		return true;
	}
	if (ACRBlockArena* Arena = ACRBlockArena::FromComponent(Hit.GetComponent()))
	{
		Arena->DamageAt(Hit.ImpactPoint, Hit.ImpactNormal, BlockDamage);
		return true;
	}
	return false;
}

float CRCombat::GetCraterRadius(const UObject* WorldContext)
{
	const UCRBenchmarkSubsystem* Bench = UCRBenchmarkSubsystem::Get(WorldContext);
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(WorldContext);
	return Bench && Bench->IsActive() ? Tuning.BenchCraterRadius : Tuning.MissileCraterRadius;
}

float CRCombat::GetGunCraterRadius(const UObject* WorldContext)
{
	const UCRBenchmarkSubsystem* Bench = UCRBenchmarkSubsystem::Get(WorldContext);
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(WorldContext);
	return Bench && Bench->IsActive() ? Tuning.BenchGunCraterRadius : Tuning.GunCraterRadius;
}

bool CRCombat::DamageWorldByGun(const FHitResult& Hit, AActor* Attacker)
{
	const float Radius = GetGunCraterRadius(Attacker);
	if (Radius > 0.f)
	{
		if (ACRBlockArena* Arena = ACRBlockArena::FromComponent(Hit.GetComponent()))
		{
			Arena->DestroySphere(Hit.ImpactPoint, Radius);
			return true;
		}
	}
	return DamageWorld(Hit, 1, Attacker);
}

namespace
{
	/** 공격력·치명타·방어력·팀 배율을 반영한 피해. bAllowCritical이면 치명타 판정. */
	float ScaleDamage(const AActor* Attacker, const ACRCharacter* Target, float Damage, bool bAllowCritical, bool& bOutCritical)
	{
		const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(Attacker);
		const UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
		const UAbilitySystemComponent* TargetASC = Target ? Target->GetAbilitySystemComponent() : nullptr;
		bOutCritical = false;
		if (SourceASC)
		{
			Damage *= FMath::Max(1.f + SourceASC->GetNumericAttribute(UCRAttributeSet::GetAttackAttribute()) / 100.f, 0.f);
			const float CritChance = SourceASC->GetNumericAttribute(UCRAttributeSet::GetCritChanceAttribute());
			if (bAllowCritical && CritChance > 0.f && FMath::FRand() * 100.f < CritChance)
			{
				Damage *= Tuning.CritMultiplier;
				bOutCritical = true;
			}
		}
		if (TargetASC)
		{
			const float Defense = FMath::Max(TargetASC->GetNumericAttribute(UCRAttributeSet::GetDefenseAttribute()), 0.f);
			const float K = FMath::Max(Tuning.DefenseConstant, 1.f);
			Damage *= K / (K + Defense);
		}
		// 팀 전투원끼리는 전투 설정의 피해 배율 (벤치마크: 싸우는 걸 보기 전에 다 죽지 않게), 웨이브 몬스터는 웨이브 배율
		if (const ACRCharacter* Source = Cast<ACRCharacter>(Attacker))
		{
			if (Source->GetTeam() == ECRTeam::Monster)
			{
				Damage *= Tuning.WaveMonsterDamageScale;
			}
			else if (Source->GetTeam() != ECRTeam::None)
			{
				if (const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(Attacker); Spawner && Spawner->IsBattleActive())
				{
					Damage *= Spawner->GetBattleSetup().DamageScale;
				}
			}
		}
		return Damage;
	}
}

bool CRCombat::IsHostile(const AActor* SourceActor, const AActor* TargetActor)
{
	const ACRCharacter* Target = Cast<ACRCharacter>(TargetActor);
	if (!Target || TargetActor == SourceActor)
	{
		return false;
	}
	const ACRCharacter* Source = Cast<ACRCharacter>(SourceActor);
	if (!Source)
	{
		return true;
	}
	// 도플갱어와 주인(그리고 같은 주인의 도플갱어끼리)은 서로 안 때린다
	const ACRCharacter* SourceSide = Source->GetHero() ? Source->GetHero() : Source;
	const ACRCharacter* TargetSide = Target->GetHero() ? Target->GetHero() : Target;
	if (SourceSide == TargetSide)
	{
		return false;
	}
	const ECRTeam SourceTeam = Source->GetTeam();
	const ECRTeam TargetTeam = Target->GetTeam();
	if (SourceTeam == ECRTeam::None)
	{
		return true;
	}
	if (SourceTeam == ECRTeam::Monster)
	{
		return TargetTeam == ECRTeam::None && Target->GetTrainingRole() == ECRTrainingRole::None;
	}
	// 레드/블루: 상대 팀만 (관전 중인 플레이어, 웨이브 몬스터 제외)
	return TargetTeam != ECRTeam::None && TargetTeam != ECRTeam::Monster && TargetTeam != SourceTeam;
}

ACRCharacter* CRCombat::FindNearestHostile(const AActor* Self, const FVector& From, float Range, const TArray<ACRCharacter*>* Exclude)
{
	UWorld* World = Self ? Self->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	World->OverlapMultiByObjectType(Overlaps, From, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Range),
		FCollisionQueryParams(SCENE_QUERY_STAT(CRFindHostile), false, Self));
	ACRCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACRCharacter* Candidate = Cast<ACRCharacter>(Overlap.GetActor());
		if (!Candidate || Candidate->IsDead() || Candidate->IsInPool() || Candidate->IsHidden() || !IsHostile(Self, Candidate)
			|| (Exclude && Exclude->Contains(Candidate)))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(From, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

void CRCombat::ApplyBurn(AActor* Attacker, ACRCharacter* Victim, float DamagePerSecond, float Duration)
{
	if (!Attacker || !Victim || Victim->IsDead() || DamagePerSecond <= 0.f || Duration <= 0.f || !IsHostile(Attacker, Victim))
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* TargetASC = Victim->GetAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return;
	}
	// 겹쳐 걸지 않고 갱신
	TargetASC->RemoveActiveGameplayEffectBySourceEffect(UCRBurnEffect::StaticClass(), nullptr);
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCRBurnEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	bool bCritical = false;
	const float TickDamage = ScaleDamage(Attacker, Victim, DamagePerSecond * UCRBurnEffect::TickPeriod, false, bCritical);
	Spec.Data->SetSetByCallerMagnitude(CRTags::Data_Damage, TickDamage);
	Spec.Data->SetSetByCallerMagnitude(CRTags::Data_Duration, Duration);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
}

bool CRCombat::ApplyWeaponHitFrom(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit, const FVector& Origin,
	float DamageOverride, ECRSkillForm Form)
{
	// 드럼통: 무기에 맞으면 HP 감소 (폭발은 한 방)
	if (ACRExplosiveBarrel* Barrel = Cast<ACRExplosiveBarrel>(Victim))
	{
		Barrel->ApplyHit(Weapon == ECRWeaponType::Missile ? 1000 : 1, Attacker);
		return true;
	}

	ACRCharacter* Target = Cast<ACRCharacter>(Victim);
	if (!Attacker || !Target || Target->IsDead() || !IsHostile(Attacker, Target))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	FCRWeaponFeel Feel = CRSkill::GetFeel(Attacker, Weapon, Form);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCRDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	// DamageOverride: 무기 수치 대신 쓰는 피해 (예: 드럼통 폭발은 미사일 넉백/연출에 자기 피해)
	const float BaseDamage = DamageOverride >= 0.f ? DamageOverride : Feel.Damage;
	Feel.Damage = ScaleDamage(Attacker, Target, BaseDamage, true, Feel.bCritical);
	if (Feel.bCritical)
	{
		Feel.HitStop *= 1.5f;
		Feel.Shake *= 1.3f;
	}
	Spec.Data->SetSetByCallerMagnitude(CRTags::Data_Damage, Feel.Damage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	if (UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(Attacker))
	{
		FVector Direction = Target->GetActorLocation() - Origin;
		Direction.Z = 0.f;
		const FVector HitLocation = Hit.bBlockingHit || Hit.ImpactPoint != FVector::ZeroVector
			? FVector(Hit.ImpactPoint)
			: Target->GetActorLocation();
		Feedback->OnHit(Attacker, Target, Weapon, Feel, HitLocation, Direction.GetSafeNormal());
	}
	return true;
}

FCollisionObjectQueryParams CRCombat::GetWeaponQueryParams()
{
	FCollisionObjectQueryParams Params;
	Params.AddObjectTypesToQuery(ECC_WorldStatic);
	Params.AddObjectTypesToQuery(ECC_Pawn);
	return Params;
}
