#include "GAS/CRDamageEffect.h"

#include "GAS/CRAttributeSet.h"
#include "GAS/CRGameplayTags.h"

UCRDamageEffect::UCRDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = CRTags::Data_Damage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCRAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}

UCRBurnEffect::UCRBurnEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration;
	Duration.DataTag = CRTags::Data_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
	Period = FScalableFloat(TickPeriod);
	bExecutePeriodicEffectOnApplication = false;

	// 재적용이 주기 타이머를 처음부터 다시 시작하면, 0.5초보다 자주 불을 맞는 동안 틱이 한 번도 안 나온다
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;

	FSetByCallerFloat TickDamage;
	TickDamage.DataTag = CRTags::Data_Damage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UCRAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(TickDamage);
	Modifiers.Add(Modifier);
}
