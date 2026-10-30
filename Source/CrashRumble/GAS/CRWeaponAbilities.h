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

/** GA_LightningFire: 지그재그 번개 히트스캔 한 줄기. */
UCLASS()
class CRASHRUMBLE_API UCRGA_LightningFire : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_MissileFire: 런처 (기본 1발 + 빨간 범위, 업그레이드로 발 수·유도·일제 사격). 판정은 미사일 폭발(범위)이 한다. */
UCLASS()
class CRASHRUMBLE_API UCRGA_MissileFire : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_FlameFire: 조준 지점 바닥부터 칸 수만큼 폴리곤 불꽃, 반경 안 대상에 피해 + 화상. */
UCLASS()
class CRASHRUMBLE_API UCRGA_FlameFire : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** GA_Punch: 왼손 펀치 (우클릭). 손을 한 프레임에 뻗고 주먹 위치에서 판정, 맞으면 때린 쪽 히트스톱. */
UCLASS()
class CRASHRUMBLE_API UCRGA_Punch : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/**
 * GA_Super: 필살기 (F). 오른손을 들고 눈에 반짝임 → 시간 정지(배경 2톤, 집중선, 줌인)
 * → 해제 순간 미사일 일제 사격 + 카나다 빛 + 임팩트 프레임.
 */
UCLASS()
class CRASHRUMBLE_API UCRGA_Super : public UCRGA_Weapon
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void HandleFreezeFinished();
};
