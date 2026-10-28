#pragma once

#include "CoreMinimal.h"
#include "SBWeaponTypes.generated.h"

UENUM(BlueprintType)
enum class ESBWeaponType : uint8
{
	None,
	Sword,
	Laser,
	Projectile
};

/** 무기별로 다른 타격감 수치 (튜닝 값에서 꺼내 씀). */
struct FSBWeaponFeel
{
	float Damage = 0.f;
	float Interval = 0.f;
	float Knockback = 0.f;
	float HitStop = 0.f;
	float Shake = 0.f;
};

class USBTuningSettings;

namespace SBWeapon
{
	SNOWBATTLE_API FSBWeaponFeel GetFeel(const USBTuningSettings& Tuning, ESBWeaponType Type);
}
