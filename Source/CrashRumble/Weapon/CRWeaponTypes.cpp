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
	case ECRWeaponType::Lightning:
		Feel = { Tuning.LightningDamage, Tuning.LightningInterval, Tuning.LightningKnockback, Tuning.LightningHitStop, Tuning.LightningShake };
		break;
	case ECRWeaponType::Missile:
		Feel = { Tuning.MissileDamage, Tuning.MissileInterval, Tuning.MissileKnockback, Tuning.MissileHitStop, Tuning.MissileShake };
		break;
	case ECRWeaponType::Flame:
		Feel = { Tuning.FlameDamage, Tuning.FlameInterval, Tuning.FlameKnockback, 0.02f, 0.4f };
		break;
	case ECRWeaponType::Punch:
		Feel = { Tuning.PunchDamage, Tuning.PunchInterval, Tuning.PunchKnockback, Tuning.PunchHitStop, Tuning.PunchShake };
		break;
	default:
		break;
	}
	return Feel;
}
