#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CRAttributeSet.generated.h"

/**
 * 체력 관련 속성.
 * Damage는 메타 속성: UCRDamageEffect가 여기에 더하면 Health에서 빼고 0으로 되돌린다.
 */
UCLASS()
class CRASHRUMBLE_API UCRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Damage);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY()
	FGameplayAttributeData Health = 100.f;

	UPROPERTY()
	FGameplayAttributeData MaxHealth = 100.f;

	UPROPERTY()
	FGameplayAttributeData Damage = 0.f;
};
