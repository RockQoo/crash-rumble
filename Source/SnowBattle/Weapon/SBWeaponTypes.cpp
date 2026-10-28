#include "Weapon/SBWeaponTypes.h"

#include "Core/SBTuningSettings.h"

FSBWeaponFeel SBWeapon::GetFeel(const USBTuningSettings& Tuning, ESBWeaponType Type)
{
	FSBWeaponFeel Feel;
	switch (Type)
	{
	case ESBWeaponType::Sword:
		Feel = { Tuning.SwordDamage, Tuning.SwordInterval, Tuning.SwordKnockback, Tuning.SwordHitStop, Tuning.SwordShake };
		break;
	case ESBWeaponType::Laser:
		Feel = { Tuning.LaserDamage, Tuning.LaserInterval, Tuning.LaserKnockback, Tuning.LaserHitStop, Tuning.LaserShake };
		break;
	case ESBWeaponType::Projectile:
		Feel = { Tuning.ProjectileDamage, Tuning.ProjectileInterval, Tuning.ProjectileKnockback, Tuning.ProjectileHitStop, Tuning.ProjectileShake };
		break;
	default:
		break;
	}
	return Feel;
}
