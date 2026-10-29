#include "GAS/CRCombat.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CRCharacter.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "GAS/CRDamageEffect.h"
#include "GAS/CRGameplayTags.h"
#include "Engine/OverlapResult.h"
#include "Map/CRBlockArena.h"
#include "Map/CRExplosiveBarrel.h"
#include "Engine/World.h"

bool CRCombat::ApplyWeaponHit(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit)
{
	return ApplyWeaponHitFrom(Attacker, Victim, Weapon, Hit, Attacker ? Attacker->GetActorLocation() : FVector::ZeroVector);
}

int32 CRCombat::ApplyRadialHit(AActor* Attacker, ECRWeaponType Weapon, const FVector& Center, float Radius)
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
		if (ApplyWeaponHitFrom(Attacker, Victim, Weapon, Hit, Center))
		{
			++Count;
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

bool CRCombat::ApplyWeaponHitFrom(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit, const FVector& Origin)
{
	// 드럼통: 무기에 맞으면 HP 감소 (폭발은 한 방)
	if (ACRExplosiveBarrel* Barrel = Cast<ACRExplosiveBarrel>(Victim))
	{
		Barrel->ApplyHit(Weapon == ECRWeaponType::Missile ? 1000 : 1, Attacker);
		return true;
	}

	ACRCharacter* Target = Cast<ACRCharacter>(Victim);
	if (!Attacker || !Target || Target == Attacker || Target->IsDead())
	{
		return false;
	}
	// 팀 전투원은 상대 팀만 때린다 (같은 팀, 관전 중인 플레이어 제외)
	const ACRCharacter* Source = Cast<ACRCharacter>(Attacker);
	if (Source && Source->GetTeam() != ECRTeam::None
		&& (Target->GetTeam() == ECRTeam::None || Target->GetTeam() == Source->GetTeam()))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	const FCRWeaponFeel Feel = CRWeapon::GetFeel(UCRTuningSubsystem::GetSettings(Attacker), Weapon);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCRDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
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
