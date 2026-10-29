#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Weapon/CRWeaponTypes.h"

class AActor;
struct FHitResult;

namespace CRCombat
{
	/**
	 * Attacker의 ASC로 UCRDamageEffect를 Victim에게 적용하고 타격감 이벤트를 보낸다.
	 * 어빌리티와 투사체(어빌리티가 이미 끝난 뒤 맞는 경우)가 같이 쓴다.
	 * @return 실제로 적용됐는지 (대상이 눈사람이 아니거나 쓰러져 있으면 false)
	 */
	CRASHRUMBLE_API bool ApplyWeaponHit(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit);

	/** 폭발처럼 공격자 위치가 아닌 Origin에서 밀어내는 타격. */
	CRASHRUMBLE_API bool ApplyWeaponHitFrom(AActor* Attacker, AActor* Victim, ECRWeaponType Weapon, const FHitResult& Hit, const FVector& Origin);

	/** Center 반경 Radius 안의 눈사람을 모두 때린다 (Origin에서 밀어냄). 맞힌 수. */
	CRASHRUMBLE_API int32 ApplyRadialHit(AActor* Attacker, ECRWeaponType Weapon, const FVector& Center, float Radius);

	/** 눈사람이 아닌 것(블럭, 드럼통)에 맞았을 때: 블럭이면 BlockDamage, 드럼통이면 1. 처리했으면 true. */
	CRASHRUMBLE_API bool DamageWorld(const FHitResult& Hit, int32 BlockDamage, AActor* Attacker);

	/** 무기 판정에 쓰는 오브젝트 타입 (바닥/벽 + 캐릭터). */
	CRASHRUMBLE_API FCollisionObjectQueryParams GetWeaponQueryParams();
}
