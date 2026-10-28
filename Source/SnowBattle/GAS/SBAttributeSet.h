#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "SBAttributeSet.generated.h"

/**
 * 체력 관련 속성.
 * Damage는 메타 속성: USBDamageEffect가 여기에 더하면 Health에서 빼고 0으로 되돌린다.
 */
UCLASS()
class SNOWBATTLE_API USBAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(USBAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(USBAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(USBAttributeSet, Damage);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY()
	FGameplayAttributeData Health = 100.f;

	UPROPERTY()
	FGameplayAttributeData MaxHealth = 100.f;

	UPROPERTY()
	FGameplayAttributeData Damage = 0.f;
};
