#pragma once

#include "CoreMinimal.h"
#include "CRWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class ECRWeaponType : uint8
{
	None,
	Sword,
	Laser,
	Projectile,
	Lightning, // 지그재그 번개 (히트스캔)
	Missile,   // 미사일 일제 사격 (이타노 서커스)
	Flame,     // 폴리곤 불꽃 (조준 지점 바닥)
	Punch      // 왼손 펀치 (장착하지 않음, 우클릭)
};

/** 무기별로 다른 타격감 수치 (튜닝 값에서 꺼내 씀). */
struct FCRWeaponFeel
{
	float Damage = 0.f;
	float Interval = 0.f;
	float Knockback = 0.f;
	float HitStop = 0.f;
	float Shake = 0.f;
};

class UCRTuningSettings;

namespace CRWeapon
{
	CRASHRUMBLE_API FCRWeaponFeel GetFeel(const UCRTuningSettings& Tuning, ECRWeaponType Type);
}
