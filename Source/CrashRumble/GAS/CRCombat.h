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

	/** 무기 판정에 쓰는 오브젝트 타입 (바닥/벽 + 캐릭터). */
	CRASHRUMBLE_API FCollisionObjectQueryParams GetWeaponQueryParams();
}
