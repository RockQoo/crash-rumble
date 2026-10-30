#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Weapon/CRSkill.h"
#include "Weapon/CRWeaponTypes.h"

class AActor;
class ACRCharacter;
struct FHitResult;

namespace CRCombat
{
	/**
	 * Source가 Target을 때릴 수 있는가.
	 * 팀 없음(플레이어·허수아비)은 누구나, 웨이브 몬스터는 팀 없는 쪽만, 레드/블루는 상대 팀만. 도플갱어와 주인은 서로 안 때린다.
	 */
	CRASHRUMBLE_API bool IsHostile(const AActor* Source, const AActor* Target);

	/**
	 * Attacker의 ASC로 UCRDamageEffect를 Victim에게 적용하고 타격감 이벤트를 보낸다.
	 * 피해 = 스킬 스탯 피해 × (1 + 공격력/100) × 치명타 × K / (K + 방어력).
	 * 어빌리티와 투사체(어빌리티가 이미 끝난 뒤 맞는 경우)가 같이 쓴다.
	 * @return 실제로 적용됐는지 (대상이 눈사람이 아니거나 쓰러져 있으면 false)
	 */
	CRASHRUMBLE_API bool ApplyWeaponHit(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit,
		ECRSkillForm Form = ECRSkillForm::Active);

	/** 폭발처럼 공격자 위치가 아닌 Origin에서 밀어내는 타격. */
	CRASHRUMBLE_API bool ApplyWeaponHitFrom(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit, const FVector& Origin,
		float DamageOverride = -1.f, ECRSkillForm Form = ECRSkillForm::Active);

	/** Center 반경 Radius 안의 눈사람을 모두 때린다 (Origin에서 밀어냄). 맞힌 대상을 OutVictims에 (있으면). 맞힌 수. */
	CRASHRUMBLE_API int32 ApplyRadialHit(AActor* Attacker, ECRWeaponType Weapon, const FVector& Center, float Radius, float DamageOverride = -1.f,
		ECRSkillForm Form = ECRSkillForm::Active, TArray<ACRCharacter*>* OutVictims = nullptr);

	/** 화상: 초당 DamagePerSecond를 Duration초 동안 (공격력·방어력 반영, 이전 화상은 갱신). */
	CRASHRUMBLE_API void ApplyBurn(AActor* Attacker, ACRCharacter* Victim, float DamagePerSecond, float Duration);

	/** From 반경 Range 안에서 Self가 때릴 수 있는 살아 있는 가장 가까운 눈사람 (Exclude 제외). */
	CRASHRUMBLE_API ACRCharacter* FindNearestHostile(const AActor* Self, const FVector& From, float Range,
		const TArray<ACRCharacter*>* Exclude = nullptr);

	/** 눈사람이 아닌 것(블럭, 드럼통)에 맞았을 때: 블럭이면 BlockDamage, 드럼통이면 1. 처리했으면 true. */
	CRASHRUMBLE_API bool DamageWorld(const FHitResult& Hit, int32 BlockDamage, AActor* Attacker);

	/** 총알·레이저가 땅에 맞았을 때: 크레이터 반경이 있으면 구멍, 없으면 블럭 1칸 피해. */
	CRASHRUMBLE_API bool DamageWorldByGun(const FHitResult& Hit, AActor* Attacker);

	/** 미사일·드럼통 폭발 크레이터 반경 (벤치마크 중이면 벤치마크 값). */
	CRASHRUMBLE_API float GetCraterRadius(const UObject* WorldContext);
	/** 총알·레이저 크레이터 반경 (0 = 블럭 1칸 피해). */
	CRASHRUMBLE_API float GetGunCraterRadius(const UObject* WorldContext);

	/** 무기 판정에 쓰는 오브젝트 타입 (바닥/벽 + 캐릭터). */
	CRASHRUMBLE_API FCollisionObjectQueryParams GetWeaponQueryParams();
}
