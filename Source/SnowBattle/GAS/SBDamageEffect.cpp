#include "GAS/SBDamageEffect.h"

#include "GAS/SBAttributeSet.h"
#include "GAS/SBGameplayTags.h"

USBDamageEffect::USBDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SBTags::Data_Damage;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USBAttributeSet::GetDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}
