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
 * 대상마다 하나만 (공격자 무관). 다시 걸면 지속시간만 갱신하고 틱 주기는 이어간다.
 * 틱 피해는 기존·새 화상 중 큰 값 (CRCombat::ApplyBurn).
 */
UCLASS()
class CRASHRUMBLE_API UCRBurnEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	static constexpr float TickPeriod = 0.5f;

	UCRBurnEffect();
};
