#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Weapon/CRSkill.h"
#include "CRGameData.generated.h"

/** 업그레이드 한 단계마다 적용: 값 = (기본 + Add × 단계) × Mul^단계. */
struct FCRStatMod
{
	ECRStat Stat = ECRStat::Damage;
	float Add = 0.f;
	float Mul = 1.f;
};

/** 업그레이드 정의 (Content/GameData/upgrades.json). 대상은 형태별 스킬 하나 (active.sword ≠ passive.sword). */
struct FCRUpgradeDef
{
	FName Id;
	FString Name;
	FString Desc;
	ECRWeaponType Family = ECRWeaponType::None;
	ECRSkillForm Form = ECRSkillForm::Active;
	TArray<FCRStatMod> Mods;
	int32 MaxLevel = 1;
	/** 이 업그레이드가 1단계 이상이어야 올릴 수 있다 (비면 없음). */
	FName Requires;
};

/** 투구 (Content/GameData/items.json). 지금은 외형만. */
struct FCRHelmetDef
{
	FName Id;
	FString Name;
	FLinearColor Color = FLinearColor::Black;
	/** cap = 기본 모자, knight = 둥근 투구, crown = 왕관. */
	FName Shape;
};

DECLARE_MULTICAST_DELEGATE(FCROnGameDataChanged);

/**
 * Content/GameData/*.json을 런타임에 직접 읽는다 (DataTable 임포트 없음, AI가 파일만 고치면 반영).
 * - "tuning": 튜닝 항목 기본값 덮어쓰기 ({ "Sword": { "Damage": 25 } } → SwordDamage, "Raw"는 이름 그대로)
 * - "upgrades": 업그레이드 정의
 * - "helmets": 투구
 * - "blocks": 블럭 표 (HP, 색)
 * 에디터·개발 빌드에서는 1초마다 파일 시각을 보고 바뀌면 다시 읽는다 (Shipping은 감시 안 함).
 * 형식: docs/게임-데이터-형식.md
 */
UCLASS()
class CRASHRUMBLE_API UCRGameDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UCRGameDataSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Reload();

	const TArray<FCRUpgradeDef>& GetUpgrades() const { return Upgrades; }
	const FCRUpgradeDef* FindUpgrade(FName Id) const;
	const TArray<FCRHelmetDef>& GetHelmets() const { return Helmets; }
	const FCRHelmetDef* FindHelmet(FName Id) const;
	/** 마지막으로 읽을 때 무시한 항목 (AI가 고칠 수 있게 파일·키 이름). */
	const TArray<FString>& GetWarnings() const { return Warnings; }

	/** 다시 읽은 뒤 (로드아웃은 스탯을 다시 계산). */
	FCROnGameDataChanged OnChanged;

	static FString GetDataDir();

private:
	bool PollFiles(float DeltaTime);
	void ParseFile(const FString& Path, TMap<FName, float>& OutTuning);

	TArray<FCRUpgradeDef> Upgrades;
	TArray<FCRHelmetDef> Helmets;
	TArray<FString> Warnings;
	TMap<FString, FDateTime> FileStamps;
	FTSTicker::FDelegateHandle PollHandle;
};
