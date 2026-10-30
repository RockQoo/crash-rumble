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

/**
 * GE_Burn: 화상. 지속시간(Data.Duration) 동안 Period마다 Data.Damage(틱당)를 Damage 메타 속성에 더한다.
 * 같은 대상에 다시 걸면 CRCombat이 이전 화상을 지우고 새로 건다 (중첩 대신 갱신).
 */
UCLASS()
class CRASHRUMBLE_API UCRBurnEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	static constexpr float TickPeriod = 0.5f;

	UCRBurnEffect();
};
