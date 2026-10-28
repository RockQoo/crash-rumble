#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SBDamageEffect.generated.h"

/** GE_Damage: 즉시 적용, SetByCaller(Data.Damage) 만큼 Damage 메타 속성에 더한다. */
UCLASS()
class SNOWBATTLE_API USBDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USBDamageEffect();
};
