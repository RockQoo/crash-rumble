#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "CRDamageEffect.generated.h"

/** GE_Damage: 즉시 적용, SetByCaller(Data.Damage) 만큼 Damage 메타 속성에 더한다. */
UCLASS()
class CRASHRUMBLE_API UCRDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UCRDamageEffect();
};
