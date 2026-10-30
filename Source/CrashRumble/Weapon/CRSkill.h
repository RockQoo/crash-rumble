#pragma once

#include "CoreMinimal.h"
#include "Weapon/CRWeaponTypes.h"

class AActor;
class UCRTuningSettings;

/** 스킬 형태. 액티브 = 내가 눌러서 사용, 패시브 = 쿨다운마다 자동 발동. 같은 계열(무기)이 두 형태를 가진다. */
enum class ECRSkillForm : uint8
{
	Active,
	Passive,
};

/**
 * 스킬 스탯. 기본값은 튜닝(→ GameData JSON), 업그레이드가 더하거나 곱한다.
 * 무기마다 의미 있는 스탯만 쓴다 (예: Chain은 번개만). 0/1 스탯(Homing, Salvo, Dual)은 동작 전환 스위치.
 */
enum class ECRStat : uint8
{
	Damage,
	Interval,   // 액티브: 발사 간격, 패시브: 쿨다운 (초)
	Knockback,
	Range,
	Count,      // 한 번에 나가는 수: 산탄 탄 수, 레이저 줄기, 미사일 발 수, 오비탈 칼날 수, 불 칸 수
	Spread,     // 여러 발일 때 퍼지는 각도 (도)
	Pierce,
	Size,       // 탄 / 빔 / 칼날 크기 배율
	Radius,     // 폭발 / 불꽃 / 궤도 반경
	Length,     // 칼 길이
	Arc,        // 칼 휘두르는 각도 (360 = 회전 베기)
	SwingTime,
	Speed,      // 투사체 속도, 궤도 회전 속도(도/초)
	Crater,     // 크레이터 반경
	Chain,      // 번개 튕기는 수
	ChainRange,
	DotDamage,  // 화상 초당 피해
	DotTime,    // 화상 지속시간
	Homing,     // 1 = 유도
	Salvo,      // 1 = 일제 사격 (이타노 서커스)
	Dual,       // 1 = 쌍검
	Num
};

struct FCRSkillStats
{
	float Values[static_cast<int32>(ECRStat::Num)] = {};

	float Get(ECRStat Stat) const { return Values[static_cast<int32>(Stat)]; }
	int32 GetInt(ECRStat Stat) const { return FMath::RoundToInt(Get(Stat)); }
	bool GetFlag(ECRStat Stat) const { return Get(Stat) > 0.5f; }
	float& operator[](ECRStat Stat) { return Values[static_cast<int32>(Stat)]; }
};

namespace CRSkill
{
	constexpr int32 NumStats = static_cast<int32>(ECRStat::Num);

	/** JSON / 콘솔용 영어 이름 ("Damage") ↔ 스탯. */
	CRASHRUMBLE_API const TCHAR* GetStatName(ECRStat Stat);
	CRASHRUMBLE_API bool FindStat(const FString& Name, ECRStat& OutStat);
	/** 화면 표시용 한글 이름. */
	CRASHRUMBLE_API FString GetStatLabel(ECRStat Stat);

	/** "sword" / "laser" / "gun" / "lightning" / "launcher" / "flame" / "punch" ↔ 무기 계열. */
	CRASHRUMBLE_API const TCHAR* GetFamilyKey(ECRWeaponType Family);
	CRASHRUMBLE_API ECRWeaponType FindFamily(const FString& Key);
	/** 칼 / 레이저 / 총 / 번개 / 런처 / 불 / 주먹. */
	CRASHRUMBLE_API FString GetFamilyLabel(ECRWeaponType Family);
	/** 인벤토리에 넣을 수 있는 스킬 계열 (주먹 제외). */
	CRASHRUMBLE_API const TArray<ECRWeaponType>& GetSkillFamilies();

	/** 스킬 아이템 ID: "active.sword", "passive.flame". */
	CRASHRUMBLE_API FName MakeSkillItem(ECRWeaponType Family, ECRSkillForm Form);
	CRASHRUMBLE_API bool ParseSkillItem(FName Item, ECRWeaponType& OutFamily, ECRSkillForm& OutForm);
	/** "active.sword" 같은 스킬 키 → 계열/형태 (업그레이드 대상 표기와 같음). */
	CRASHRUMBLE_API bool ParseSkillKey(const FString& Key, ECRWeaponType& OutFamily, ECRSkillForm& OutForm);

	/** 튜닝 값으로 만든 기본 스탯 (업그레이드 없음). */
	CRASHRUMBLE_API FCRSkillStats MakeBaseStats(const UCRTuningSettings& Tuning, ECRWeaponType Family, ECRSkillForm Form);

	/** Owner가 로드아웃을 가진 캐릭터면 업그레이드가 반영된 스탯, 아니면 기본 스탯. */
	CRASHRUMBLE_API FCRSkillStats GetStats(const AActor* Owner, ECRWeaponType Family, ECRSkillForm Form);

	/** 타격감 수치: 피해 / 간격 / 넉백은 스탯, 히트스톱 / 흔들림은 튜닝. */
	CRASHRUMBLE_API FCRWeaponFeel GetFeel(const AActor* Owner, ECRWeaponType Family, ECRSkillForm Form);

	/** 공격속도 스탯을 반영한 간격 (간격 ÷ (1 + 공격속도/100)). */
	CRASHRUMBLE_API float GetAttackInterval(const AActor* Owner, float BaseInterval);
}
