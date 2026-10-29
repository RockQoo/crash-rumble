#include "Weapon/CRWeaponTypes.h"

#include "Core/CRTuningSettings.h"

FCRWeaponFeel CRWeapon::GetFeel(const UCRTuningSettings& Tuning, ECRWeaponType Type)
{
	FCRWeaponFeel Feel;
	switch (Type)
	{
	case ECRWeaponType::Sword:
		Feel = { Tuning.SwordDamage, Tuning.SwordInterval, Tuning.SwordKnockback, Tuning.SwordHitStop, Tuning.SwordShake };
		break;
	case ECRWeaponType::Laser:
		Feel = { Tuning.LaserDamage, Tuning.LaserInterval, Tuning.LaserKnockback, Tuning.LaserHitStop, Tuning.LaserShake };
		break;
	case ECRWeaponType::Projectile:
		Feel = { Tuning.ProjectileDamage, Tuning.ProjectileInterval, Tuning.ProjectileKnockback, Tuning.ProjectileHitStop, Tuning.ProjectileShake };
		break;
	default:
		break;
	}
	return Feel;
}
