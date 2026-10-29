#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CRWeaponAbilities.generated.h"

class ACRCharacter;
class UCRWeaponComponent;

/** 무기 어빌리티 공통. 공격 간격은 UCRWeaponComponent가 관리한다. */
UCLASS(Abstract)
class CRASHRUMBLE_API UCRGA_Weapon : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UCRGA_Weapon();

protected:
	ACRCharacter* GetSnowman() const;
	UCRWeaponComponent* GetWeapon() const;
	void FinishAbility(bool bCancelled = false);
};

/** GA_SwordSwing: 손을 호 궤적으로 휘두르며 칼날 스윕 판정. 휘두르기가 끝나면 종료. */
UCLASS()
class CRASHRUMBLE_API UCRGA_SwordSwing : public UCRGA_Weapon
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
class CRASHRUMBLE_API UCRGA_LaserFire : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_ProjectileFire: 풀에서 투사체 한 발 발사. 명중 처리는 투사체가 한다. */
UCLASS()
class CRASHRUMBLE_API UCRGA_ProjectileFire : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
