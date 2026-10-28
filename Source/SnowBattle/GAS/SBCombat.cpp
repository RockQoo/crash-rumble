#include "GAS/SBCombat.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/SBSnowmanCharacter.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Feedback/SBFeedbackSubsystem.h"
#include "GAS/SBDamageEffect.h"
#include "GAS/SBGameplayTags.h"

bool SBCombat::ApplyWeaponHit(AActor* Attacker, AActor* Victim, ESBWeaponType Weapon, const FHitResult& Hit)
{
	ASBSnowmanCharacter* Target = Cast<ASBSnowmanCharacter>(Victim);
	if (!Attacker || !Target || Target == Attacker || Target->IsDead())
	{
		return false;
	}
	// 팀 전투원은 상대 팀만 때린다 (같은 팀, 관전 중인 플레이어 제외)
	const ASBSnowmanCharacter* Source = Cast<ASBSnowmanCharacter>(Attacker);
	if (Source && Source->GetTeam() != ESBTeam::None
		&& (Target->GetTeam() == ESBTeam::None || Target->GetTeam() == Source->GetTeam()))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	const FSBWeaponFeel Feel = SBWeapon::GetFeel(USBTuningSubsystem::GetSettings(Attacker), Weapon);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddHitResult(Hit, true);
	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(USBDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(SBTags::Data_Damage, Feel.Damage);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	if (USBFeedbackSubsystem* Feedback = USBFeedbackSubsystem::Get(Attacker))
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

FCollisionObjectQueryParams SBCombat::GetWeaponQueryParams()
{
	FCollisionObjectQueryParams Params;
	Params.AddObjectTypesToQuery(ECC_WorldStatic);
	Params.AddObjectTypesToQuery(ECC_Pawn);
	return Params;
}
