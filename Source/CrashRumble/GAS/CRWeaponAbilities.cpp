#include "GAS/CRWeaponAbilities.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "Feedback/CRFxSubsystem.h"
#include "Feedback/CRScreenFxSubsystem.h"
#include "GAS/CRCombat.h"
#include "Weapon/CRWeaponComponent.h"

namespace
{
	// 필살기: 오른손을 머리 위로
	const FVector SuperHandLocation(10.f, 30.f, 95.f);
	const FRotator SuperHandRotation(80.f, 0.f, 0.f);
	constexpr float SuperFocusAfter = 0.5f;
}

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

void UCRGA_LightningFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	TArray<FHitResult> Hits;
	Weapon->FireLightning(Hits);
	for (const FHitResult& Hit : Hits)
	{
		CRCombat::ApplyWeaponHit(GetAvatarActorFromActorInfo(), Hit.GetActor(), ECRWeaponType::Lightning, Hit);
	}
	FinishAbility();
}

void UCRGA_MissileFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Weapon->FireMissiles(UCRTuningSubsystem::GetSettings(Weapon).MissileCount);
	FinishAbility();
}

void UCRGA_FlameFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UCRWeaponComponent* Weapon = GetWeapon();
	if (!Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(Weapon);
	const FVector Ground = Weapon->FireFlame();
	// 불꽃 기둥 가운데 높이에서 반경 판정
	CRCombat::ApplyRadialHit(GetAvatarActorFromActorInfo(), ECRWeaponType::Flame, Ground + FVector(0.f, 0.f, Tuning.FlameHeight * 0.35f),
		Tuning.FlameRadius + 40.f);
	FinishAbility();
}

void UCRGA_Punch::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACRCharacter* Snowman = GetSnowman();
	if (!Snowman || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	Snowman->PerformPunch();
	FinishAbility();
}

void UCRGA_Super::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACRCharacter* Snowman = GetSnowman();
	UCRWeaponComponent* Weapon = GetWeapon();
	UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(Snowman);
	if (!Snowman || !Weapon || !Feedback || Feedback->IsTimeFrozen() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(true);
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(Snowman);

	// 1. 무기를 미사일로, 오른손을 머리 위로, 눈에 십자 반짝임 (시간 정지 중에도 보이게 실시간)
	Weapon->Equip(ECRWeaponType::Missile);
	Weapon->SetHandOverride(SuperHandLocation, SuperHandRotation, Tuning.SuperFreezeDuration + 0.2f);
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(Snowman); Fx && Tuning.bSparkle)
	{
		for (const FName Eye : { FName(TEXT("EyeL")), FName(TEXT("EyeR")) })
		{
			if (UStaticMeshComponent* EyePart = Snowman->FindPart(Eye))
			{
				Fx->SpawnSparkle(EyePart->GetComponentLocation(), 22.f, EyePart, true);
			}
		}
	}

	// 2. 시간 정지: 배경 2톤 + 집중선 + 줌인 (캐릭터만 컬러로 계속 숨쉬며 움직인다)
	if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(Snowman))
	{
		Screen->AddFocusLines(1.f, Tuning.SuperFreezeDuration);
	}
	Snowman->GetViewComponent()->SetFovScale(Tuning.SuperZoom);
	Feedback->StartTimeFreeze(Snowman, Tuning.SuperFreezeDuration, FSimpleDelegate::CreateUObject(this, &UCRGA_Super::HandleFreezeFinished));
}

void UCRGA_Super::HandleFreezeFinished()
{
	ACRCharacter* Snowman = GetSnowman();
	UCRWeaponComponent* Weapon = GetWeapon();
	if (Snowman && Weapon)
	{
		const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(Snowman);
		Snowman->GetViewComponent()->SetFovScale(1.f);

		// 3. 해제 순간: 미사일 일제 사격(간격 절반), 총구에 카나다 빛, 임팩트 프레임(쿨다운 무시), 집중선, 화면 흔들림
		Weapon->FireMissiles(Tuning.SuperMissileCount, true);
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(Snowman); Fx && Tuning.bKanada)
		{
			Fx->SpawnKanada(Weapon->GetMuzzleLocation(), 140.f);
		}
		if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(Snowman))
		{
			Screen->RequestImpactFrame(true);
			Screen->AddFocusLines(1.f, SuperFocusAfter);
		}
		if (Tuning.bCameraShake)
		{
			Snowman->GetViewComponent()->AddShake(1.5f);
		}
	}
	if (IsActive())
	{
		FinishAbility();
	}
}
