#include "GAS/CRCombat.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CRCharacter.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "GAS/CRDamageEffect.h"
#include "GAS/CRGameplayTags.h"

bool CRCombat::ApplyWeaponHit(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit)
{
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
		FVector Direction = Target->GetActorLocation() - Attacker->GetActorLocation();
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
