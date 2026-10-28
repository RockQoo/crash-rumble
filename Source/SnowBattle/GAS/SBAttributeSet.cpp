#include "GAS/SBAttributeSet.h"

#include "GameplayEffectExtension.h"

void USBAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void USBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float Amount = GetDamage();
		SetDamage(0.f);
		if (Amount > 0.f && GetHealth() > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() - Amount, 0.f, GetMaxHealth()));
		}
	}
}
