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
