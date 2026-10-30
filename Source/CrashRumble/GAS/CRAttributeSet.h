#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CRAttributeSet.generated.h"

/**
 * 체력과 캐릭터 스탯 (공격력·공격속도·방어력·이동속도는 %, 치명타 확률 %).
 * Damage는 메타 속성: UCRDamageEffect / 화상(UCRBurnEffect)이 여기에 더하면 Health에서 빼고 0으로 되돌린다.
 * 공격력·치명타·방어력 계산은 CRCombat이 피해를 넣기 전에 한다 (화상 틱은 넣을 때 정해진 값).
 */
UCLASS()
class CRASHRUMBLE_API UCRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Damage);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Attack);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, AttackSpeed);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, Defense);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, MoveSpeed);
	ATTRIBUTE_ACCESSORS_BASIC(UCRAttributeSet, CritChance);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY()
	FGameplayAttributeData Health = 100.f;

	UPROPERTY()
	FGameplayAttributeData MaxHealth = 100.f;

	UPROPERTY()
	FGameplayAttributeData Damage = 0.f;

	UPROPERTY()
	FGameplayAttributeData Attack = 0.f;

	UPROPERTY()
	FGameplayAttributeData AttackSpeed = 0.f;

	UPROPERTY()
	FGameplayAttributeData Defense = 0.f;

	UPROPERTY()
	FGameplayAttributeData MoveSpeed = 0.f;

	UPROPERTY()
	FGameplayAttributeData CritChance = 0.f;
};
