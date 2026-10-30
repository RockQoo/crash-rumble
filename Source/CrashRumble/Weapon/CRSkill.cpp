#include "Weapon/CRSkill.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CRCharacter.h"
#include "Character/CRLoadoutComponent.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "GAS/CRAttributeSet.h"

namespace
{
	struct FStatInfo
	{
		const TCHAR* Name;
		const TCHAR* Label;
	};

	const FStatInfo StatInfos[] = {
		{ TEXT("Damage"), TEXT("피해") },
		{ TEXT("Interval"), TEXT("간격/쿨다운") },
		{ TEXT("Knockback"), TEXT("넉백") },
		{ TEXT("Range"), TEXT("사거리") },
		{ TEXT("Count"), TEXT("개수") },
		{ TEXT("Spread"), TEXT("퍼짐 각도") },
		{ TEXT("Pierce"), TEXT("관통") },
		{ TEXT("Size"), TEXT("크기") },
		{ TEXT("Radius"), TEXT("반경") },
		{ TEXT("Length"), TEXT("길이") },
		{ TEXT("Arc"), TEXT("휘두르는 각도") },
		{ TEXT("SwingTime"), TEXT("휘두르는 시간") },
		{ TEXT("Speed"), TEXT("속도") },
		{ TEXT("Crater"), TEXT("크레이터") },
		{ TEXT("Chain"), TEXT("체인") },
		{ TEXT("ChainRange"), TEXT("체인 거리") },
		{ TEXT("DotDamage"), TEXT("화상 초당 피해") },
		{ TEXT("DotTime"), TEXT("화상 시간") },
		{ TEXT("Homing"), TEXT("유도") },
		{ TEXT("Salvo"), TEXT("일제 사격") },
		{ TEXT("Dual"), TEXT("쌍검") },
	};
	static_assert(UE_ARRAY_COUNT(StatInfos) == CRSkill::NumStats, "스탯 이름 표와 enum 개수가 같아야 함");

	struct FFamilyInfo
	{
		ECRWeaponType Family;
		const TCHAR* Key;
		const TCHAR* Label;
	};

	const FFamilyInfo Families[] = {
		{ ECRWeaponType::Sword, TEXT("sword"), TEXT("칼") },
		{ ECRWeaponType::Laser, TEXT("laser"), TEXT("레이저") },
		{ ECRWeaponType::Projectile, TEXT("gun"), TEXT("총") },
		{ ECRWeaponType::Lightning, TEXT("lightning"), TEXT("번개") },
		{ ECRWeaponType::Missile, TEXT("launcher"), TEXT("런처") },
		{ ECRWeaponType::Flame, TEXT("flame"), TEXT("불") },
		{ ECRWeaponType::Punch, TEXT("punch"), TEXT("주먹") },
	};

	const TCHAR* ActivePrefix = TEXT("active.");
	const TCHAR* PassivePrefix = TEXT("passive.");
}

const TCHAR* CRSkill::GetStatName(ECRStat Stat)
{
	const int32 Index = static_cast<int32>(Stat);
	return Index >= 0 && Index < NumStats ? StatInfos[Index].Name : TEXT("?");
}

bool CRSkill::FindStat(const FString& Name, ECRStat& OutStat)
{
	for (int32 i = 0; i < NumStats; ++i)
	{
		if (Name.Equals(StatInfos[i].Name, ESearchCase::IgnoreCase))
		{
			OutStat = static_cast<ECRStat>(i);
			return true;
		}
	}
	return false;
}

FString CRSkill::GetStatLabel(ECRStat Stat)
{
	const int32 Index = static_cast<int32>(Stat);
	return Index >= 0 && Index < NumStats ? StatInfos[Index].Label : TEXT("?");
}

const TCHAR* CRSkill::GetFamilyKey(ECRWeaponType Family)
{
	for (const FFamilyInfo& Info : Families)
	{
		if (Info.Family == Family)
		{
			return Info.Key;
		}
	}
	return TEXT("none");
}

ECRWeaponType CRSkill::FindFamily(const FString& Key)
{
	for (const FFamilyInfo& Info : Families)
	{
		if (Key.Equals(Info.Key, ESearchCase::IgnoreCase))
		{
			return Info.Family;
		}
	}
	return ECRWeaponType::None;
}

FString CRSkill::GetFamilyLabel(ECRWeaponType Family)
{
	for (const FFamilyInfo& Info : Families)
	{
		if (Info.Family == Family)
		{
			return Info.Label;
		}
	}
	return TEXT("");
}

const TArray<ECRWeaponType>& CRSkill::GetSkillFamilies()
{
	static const TArray<ECRWeaponType> Result = { ECRWeaponType::Sword, ECRWeaponType::Laser, ECRWeaponType::Projectile,
		ECRWeaponType::Lightning, ECRWeaponType::Missile, ECRWeaponType::Flame };
	return Result;
}

FName CRSkill::MakeSkillItem(ECRWeaponType Family, ECRSkillForm Form)
{
	return FName(FString(Form == ECRSkillForm::Active ? ActivePrefix : PassivePrefix) + GetFamilyKey(Family));
}

bool CRSkill::ParseSkillKey(const FString& Key, ECRWeaponType& OutFamily, ECRSkillForm& OutForm)
{
	FString Rest;
	if (Key.StartsWith(ActivePrefix, ESearchCase::IgnoreCase))
	{
		OutForm = ECRSkillForm::Active;
		Rest = Key.Mid(FCString::Strlen(ActivePrefix));
	}
	else if (Key.StartsWith(PassivePrefix, ESearchCase::IgnoreCase))
	{
		OutForm = ECRSkillForm::Passive;
		Rest = Key.Mid(FCString::Strlen(PassivePrefix));
	}
	else
	{
		return false;
	}
	OutFamily = FindFamily(Rest);
	return OutFamily != ECRWeaponType::None;
}

bool CRSkill::ParseSkillItem(FName Item, ECRWeaponType& OutFamily, ECRSkillForm& OutForm)
{
	return !Item.IsNone() && ParseSkillKey(Item.ToString(), OutFamily, OutForm);
}

FCRSkillStats CRSkill::MakeBaseStats(const UCRTuningSettings& T, ECRWeaponType Family, ECRSkillForm Form)
{
	FCRSkillStats S;
	S[ECRStat::Size] = 1.f;
	S[ECRStat::Count] = 1.f;
	switch (Family)
	{
	case ECRWeaponType::Sword:
		S[ECRStat::Damage] = T.SwordDamage;
		S[ECRStat::Interval] = T.SwordInterval;
		S[ECRStat::Knockback] = T.SwordKnockback;
		S[ECRStat::Length] = T.SwordLength;
		S[ECRStat::Arc] = T.SwordArcAngle;
		S[ECRStat::SwingTime] = T.SwordSwingDuration;
		S[ECRStat::Dual] = T.SwordDual;
		break;
	case ECRWeaponType::Laser:
		S[ECRStat::Damage] = T.LaserDamage;
		S[ECRStat::Interval] = T.LaserInterval;
		S[ECRStat::Knockback] = T.LaserKnockback;
		S[ECRStat::Range] = T.LaserRange;
		S[ECRStat::Count] = T.LaserBeams;
		S[ECRStat::Spread] = T.LaserSpread;
		S[ECRStat::Pierce] = T.LaserPierce;
		break;
	case ECRWeaponType::Projectile:
		S[ECRStat::Damage] = T.ProjectileDamage;
		S[ECRStat::Interval] = T.ProjectileInterval;
		S[ECRStat::Knockback] = T.ProjectileKnockback;
		S[ECRStat::Count] = T.ProjectilePellets;
		S[ECRStat::Spread] = T.ProjectileSpread;
		S[ECRStat::Pierce] = T.ProjectilePierce;
		S[ECRStat::Size] = T.ProjectileSize;
		S[ECRStat::Speed] = T.ProjectileSpeed;
		S[ECRStat::Range] = T.ProjectileSpeed * T.ProjectileLifetime;
		break;
	case ECRWeaponType::Lightning:
		S[ECRStat::Damage] = T.LightningDamage;
		S[ECRStat::Interval] = T.LightningInterval;
		S[ECRStat::Knockback] = T.LightningKnockback;
		S[ECRStat::Range] = T.LightningRange;
		S[ECRStat::Chain] = T.LightningChain;
		S[ECRStat::ChainRange] = T.LightningChainRange;
		break;
	case ECRWeaponType::Missile:
		S[ECRStat::Damage] = T.MissileDamage;
		S[ECRStat::Interval] = T.MissileInterval;
		S[ECRStat::Knockback] = T.MissileKnockback;
		S[ECRStat::Count] = T.MissileCount;
		S[ECRStat::Radius] = T.MissileRadius;
		S[ECRStat::Crater] = T.MissileCraterRadius;
		S[ECRStat::Homing] = T.MissileHoming;
		S[ECRStat::Salvo] = T.MissileSalvo;
		S[ECRStat::Range] = T.MissileRange;
		break;
	case ECRWeaponType::Flame:
		S[ECRStat::Damage] = T.FlameDamage;
		S[ECRStat::Interval] = T.FlameInterval;
		S[ECRStat::Knockback] = T.FlameKnockback;
		S[ECRStat::Radius] = T.FlameRadius;
		S[ECRStat::Range] = T.FlameRange;
		S[ECRStat::Count] = T.FlameTiles;
		S[ECRStat::Length] = T.FlameTileSpacing;
		S[ECRStat::DotDamage] = T.FlameDotDamage;
		S[ECRStat::DotTime] = T.FlameDotTime;
		break;
	case ECRWeaponType::Punch:
		S[ECRStat::Damage] = T.PunchDamage;
		S[ECRStat::Interval] = T.PunchInterval;
		S[ECRStat::Knockback] = T.PunchKnockback;
		S[ECRStat::Radius] = T.PunchRadius;
		S[ECRStat::Range] = T.PunchReach;
		break;
	default:
		break;
	}

	if (Form == ECRSkillForm::Passive)
	{
		// 패시브: 피해·쿨다운은 따로, 한 번에 하나씩 (늘리는 건 패시브 업그레이드)
		S[ECRStat::Range] = T.PassiveRange;
		S[ECRStat::Count] = 1.f;
		S[ECRStat::Salvo] = 0.f;
		switch (Family)
		{
		case ECRWeaponType::Sword:
			S[ECRStat::Damage] = T.PassiveSwordDamage;
			S[ECRStat::Interval] = T.PassiveSwordInterval;
			S[ECRStat::Count] = T.PassiveSwordCount;
			S[ECRStat::Radius] = T.PassiveSwordRadius;
			S[ECRStat::Speed] = T.PassiveSwordSpeed;
			S[ECRStat::Size] = T.PassiveSwordSize;
			break;
		case ECRWeaponType::Laser:
			S[ECRStat::Damage] = T.PassiveLaserDamage;
			S[ECRStat::Interval] = T.PassiveLaserInterval;
			break;
		case ECRWeaponType::Projectile:
			S[ECRStat::Damage] = T.PassiveProjectileDamage;
			S[ECRStat::Interval] = T.PassiveProjectileInterval;
			break;
		case ECRWeaponType::Lightning:
			S[ECRStat::Damage] = T.PassiveLightningDamage;
			S[ECRStat::Interval] = T.PassiveLightningInterval;
			break;
		case ECRWeaponType::Missile:
			S[ECRStat::Damage] = T.PassiveMissileDamage;
			S[ECRStat::Interval] = T.PassiveMissileInterval;
			break;
		case ECRWeaponType::Flame:
			S[ECRStat::Damage] = T.PassiveFlameDamage;
			S[ECRStat::Interval] = T.PassiveFlameInterval;
			break;
		default:
			break;
		}
	}
	return S;
}

FCRSkillStats CRSkill::GetStats(const AActor* Owner, ECRWeaponType Family, ECRSkillForm Form)
{
	if (const ACRCharacter* Snowman = Cast<ACRCharacter>(Owner))
	{
		if (const UCRLoadoutComponent* Loadout = Snowman->GetLoadout())
		{
			return Loadout->GetStats(Family, Form);
		}
	}
	return MakeBaseStats(UCRTuningSubsystem::GetSettings(Owner), Family, Form);
}

FCRWeaponFeel CRSkill::GetFeel(const AActor* Owner, ECRWeaponType Family, ECRSkillForm Form)
{
	FCRWeaponFeel Feel = CRWeapon::GetFeel(UCRTuningSubsystem::GetSettings(Owner), Family);
	if (Family == ECRWeaponType::None)
	{
		return Feel;
	}
	const FCRSkillStats Stats = GetStats(Owner, Family, Form);
	Feel.Damage = Stats.Get(ECRStat::Damage);
	Feel.Interval = Stats.Get(ECRStat::Interval);
	Feel.Knockback = Stats.Get(ECRStat::Knockback);
	return Feel;
}

float CRSkill::GetAttackInterval(const AActor* Owner, float BaseInterval)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const float Bonus = ASC ? ASC->GetNumericAttribute(UCRAttributeSet::GetAttackSpeedAttribute()) : 0.f;
	return BaseInterval / FMath::Max(1.f + Bonus / 100.f, 0.1f);
}
