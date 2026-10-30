#include "GAS/CRAttributeSet.h"

#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "GameplayEffectExtension.h"
#include "GAS/CRDamageEffect.h"

void UCRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UCRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float Amount = GetDamage();
		SetDamage(0.f);
		if (Amount > 0.f && GetHealth() > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Amount, 0.f, GetMaxHealth()));

			// 화상 틱: 작은 데미지 숫자 (일반 타격 숫자는 CRCombat → 타격감 서브시스템이 띄운다)
			const AActor* Avatar = Data.Target.AbilityActorInfo.IsValid() ? Data.Target.AbilityActorInfo->AvatarActor.Get() : nullptr;
			if (Avatar && Data.EffectSpec.Def && Data.EffectSpec.Def->IsA<UCRBurnEffect>()
				&& UCRTuningSubsystem::GetSettings(Avatar).bDamageNumbers)
			{
				if (UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(Avatar))
				{
					Feedback->AddDamageNumber(Avatar->GetActorLocation() + FVector(0.f, 0.f, 40.f), Amount, ECRWeaponType::Flame, 0.7f);
				}
			}
		}
	}
}
