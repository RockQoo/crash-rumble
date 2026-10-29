#pragma once

#include "CoreMinimal.h"
#include "CRWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class ECRWeaponType : uint8
{
	None,
	Sword,
	Laser,
	Projectile
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
