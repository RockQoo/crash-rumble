#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SBWeaponAbilities.generated.h"

class ASBSnowmanCharacter;
class USBWeaponComponent;

/** 무기 어빌리티 공통. 공격 간격은 USBWeaponComponent가 관리한다. */
UCLASS(Abstract)
class SNOWBATTLE_API USBGA_Weapon : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USBGA_Weapon();

protected:
	ASBSnowmanCharacter* GetSnowman() const;
	USBWeaponComponent* GetWeapon() const;
	void FinishAbility(bool bCancelled = false);
};

/** GA_SwordSwing: 손을 호 궤적으로 휘두르며 칼날 스윕 판정. 휘두르기가 끝나면 종료. */
UCLASS()
class SNOWBATTLE_API USBGA_SwordSwing : public USBGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void HandleSwingHit(const FHitResult& Hit);
	void HandleSwingFinished();
};

/** GA_LaserFire: 히트스캔 한 발. */
UCLASS()
class SNOWBATTLE_API USBGA_LaserFire : public USBGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_ProjectileFire: 풀에서 투사체 한 발 발사. 명중 처리는 투사체가 한다. */
UCLASS()
class SNOWBATTLE_API USBGA_ProjectileFire : public USBGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
