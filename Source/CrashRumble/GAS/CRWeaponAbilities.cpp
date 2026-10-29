#include "GAS/CRWeaponAbilities.h"

#include "Character/CRCharacter.h"
#include "GAS/CRCombat.h"
#include "Weapon/CRWeaponComponent.h"

UCRGA_Weapon::UCRGA_Weapon()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ACRCharacter* UCRGA_Weapon::GetSnowman() const
{
	return Cast<ACRCharacter>(GetAvatarActorFromActorInfo());
}

UCRWeaponComponent* UCRGA_Weapon::GetWeapon() const
{
	const ACRCharacter* Snowman = GetSnowman();
	return Snowman ? Snowman->GetWeaponComponent() : nullptr;
}

void UCRGA_Weapon::FinishAbility(bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UCRGA_SwordSwing::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Weapon->StartSwing(
		FCRSwingHitDelegate::CreateUObject(this, &UCRGA_SwordSwing::HandleSwingHit),
		FSimpleDelegate::CreateUObject(this, &UCRGA_SwordSwing::HandleSwingFinished));
}

void UCRGA_SwordSwing::HandleSwingHit(const FHitResult& Hit)
{
	CRCombat::ApplyWeaponHit(GetAvatarActorFromActorInfo(), Hit.GetActor(), ECRWeaponType::Sword, Hit);
}

void UCRGA_SwordSwing::HandleSwingFinished()
{
	if (IsActive())
	{
		FinishAbility();
	}
}

void UCRGA_LaserFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	TArray<FHitResult> Hits;
	Weapon->FireLaser(Hits);
	for (const FHitResult& Hit : Hits)
	{
		CRCombat::ApplyWeaponHit(GetAvatarActorFromActorInfo(), Hit.GetActor(), ECRWeaponType::Laser, Hit);
	}
	FinishAbility();
}

void UCRGA_ProjectileFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Weapon->FireProjectile();
	FinishAbility();
}
