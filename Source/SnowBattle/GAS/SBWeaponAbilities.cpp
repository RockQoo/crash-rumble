#include "GAS/SBWeaponAbilities.h"

#include "Character/SBSnowmanCharacter.h"
#include "GAS/SBCombat.h"
#include "Weapon/SBWeaponComponent.h"

USBGA_Weapon::USBGA_Weapon()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

ASBSnowmanCharacter* USBGA_Weapon::GetSnowman() const
{
	return Cast<ASBSnowmanCharacter>(GetAvatarActorFromActorInfo());
}

USBWeaponComponent* USBGA_Weapon::GetWeapon() const
{
	const ASBSnowmanCharacter* Snowman = GetSnowman();
	return Snowman ? Snowman->GetWeaponComponent() : nullptr;
}

void USBGA_Weapon::FinishAbility(bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void USBGA_SwordSwing::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	USBWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Weapon->StartSwing(
		FSBSwingHitDelegate::CreateUObject(this, &USBGA_SwordSwing::HandleSwingHit),
		FSimpleDelegate::CreateUObject(this, &USBGA_SwordSwing::HandleSwingFinished));
}

void USBGA_SwordSwing::HandleSwingHit(const FHitResult& Hit)
{
	SBCombat::ApplyWeaponHit(GetAvatarActorFromActorInfo(), Hit.GetActor(), ESBWeaponType::Sword, Hit);
}

void USBGA_SwordSwing::HandleSwingFinished()
{
	if (IsActive())
	{
		FinishAbility();
	}
}

void USBGA_LaserFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	USBWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}

	FHitResult Hit;
	if (Weapon->FireLaser(Hit))
	{
		SBCombat::ApplyWeaponHit(GetAvatarActorFromActorInfo(), Hit.GetActor(), ESBWeaponType::Laser, Hit);
	}
	FinishAbility();
}

void USBGA_ProjectileFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	USBWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Weapon->FireProjectile();
	FinishAbility();
}
