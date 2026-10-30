#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/CRSkill.h"
#include "CRLoadoutComponent.generated.h"

class ACRCharacter;
class UStaticMeshComponent;

/** 인벤토리 UI가 가리키는 칸. 장착 칸은 지금 프리셋 기준. */
enum class ECRSlotKind : uint8
{
	Inventory,
	Active,
	Passive,
	Helmet,
};

struct FCRSlotRef
{
	ECRSlotKind Kind = ECRSlotKind::Inventory;
	int32 Index = 0;

	bool operator==(const FCRSlotRef& Other) const { return Kind == Other.Kind && Index == Other.Index; }
};

/** 장비 프리셋 하나: 액티브 2칸(좌클릭 / 우클릭) + 패시브 4칸. 아이템 ID ("active.sword"), 비면 None. */
struct FCRLoadoutPreset
{
	static constexpr int32 ActiveCount = 2;
	static constexpr int32 PassiveCount = 4;
	FName Active[ActiveCount];
	FName Passive[PassiveCount];
};

/**
 * 캐릭터의 장비와 성장: 인벤토리(6×4), 장비 프리셋 2개(X로 교체), 투구, 업그레이드 단계, 골드.
 * 스킬 스탯 = 튜닝 기본값 + 업그레이드 (형태별로 캐시). 패시브 칸의 스킬은 여기서 쿨다운마다 자동 발동한다.
 * 플레이어·도플갱어·몬스터 모두 가지고 있다 (몬스터는 업그레이드 없이 기본 스탯).
 */
UCLASS(ClassGroup = (CrashRumble))
class CRASHRUMBLE_API UCRLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 InventoryColumns = 6;
	static constexpr int32 InventoryRows = 4;
	static constexpr int32 InventorySize = InventoryColumns * InventoryRows;
	static constexpr int32 PresetCount = 2;

	UCRLoadoutComponent();

	/** 시작 아이템: 스킬 6종의 액티브·패시브 + 투구들. 프리셋 1 좌클릭 칸에 칼. */
	void GiveStarterItems();

	// 아이템 / 장착
	FName GetSlotItem(const FCRSlotRef& Slot) const;
	bool CanPlace(FName Item, const FCRSlotRef& Slot) const;
	/** From → To로 옮긴다 (To에 있던 것은 From으로, 못 들어가면 실패). */
	bool MoveItem(const FCRSlotRef& From, const FCRSlotRef& To);
	/** 우클릭: 인벤토리 아이템은 맞는 장착 칸(빈 칸, 없으면 첫 칸과 교체)으로, 장착된 아이템은 인벤토리 빈 칸으로. */
	bool QuickMove(const FCRSlotRef& From);
	/** 숫자키(훈련장): 이 계열 액티브를 좌클릭 칸에 (인벤토리에 없으면 만들어서). */
	void QuickEquipActive(ECRWeaponType Family);
	ECRWeaponType GetActiveFamily(int32 Slot) const;
	ECRWeaponType GetPassiveFamily(int32 Slot) const;
	FName GetHelmet() const { return Helmet; }
	int32 GetPresetIndex() const { return PresetIndex; }
	/** X: 프리셋 1 ↔ 2 (쿨다운). 바꿨으면 true. */
	bool SwapPreset();
	/** 패시브 칸 남은 쿨다운 비율 (0 = 준비됨). */
	float GetPassiveCooldownRatio(int32 Slot) const;

	// 업그레이드 / 스탯
	FCRSkillStats GetStats(ECRWeaponType Family, ECRSkillForm Form) const;
	int32 GetUpgradeLevel(FName Id) const;
	/** 최대 단계·선행 조건을 확인해서 바꾼다. 바뀌었으면 true (Message에 이유). */
	bool SetUpgradeLevel(FName Id, int32 Level, FString* OutMessage = nullptr);
	void ResetUpgrades();
	const TMap<FName, int32>& GetUpgradeLevels() const { return UpgradeLevels; }
	void InvalidateStats() { StatCache.Reset(); }

	// 골드
	int32 GetGold() const { return Gold; }
	/** 화면 표시용: 실제 골드를 드르륵 따라간다. */
	int32 GetDisplayedGold() const { return FMath::RoundToInt(DisplayedGold); }
	void AddGold(int32 Amount);

	/** 도플갱어: 장착·투구·업그레이드를 복사 (인벤토리·골드 제외). */
	void CopyEquipmentFrom(const UCRLoadoutComponent& Other);
	void ClearEquipment();

	/** 장착이 바뀜 (무기·투구 외형 갱신). */
	FSimpleMulticastDelegate OnEquipmentChanged;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FName* FindSlot(const FCRSlotRef& Slot);
	const FName* FindSlot(const FCRSlotRef& Slot) const;
	int32 FindEmptyInventory() const;
	void EnsureHelmet();
	void NotifyEquipmentChanged();

	void UpdatePassives(float DeltaTime);
	bool FirePassive(ECRWeaponType Family, const FCRSkillStats& Stats);
	void UpdateOrbit(float DeltaTime, const FCRSkillStats* Stats);
	ACRCharacter* GetSnowman() const;

	TArray<FName> Inventory;
	FCRLoadoutPreset Presets[PresetCount];
	int32 PresetIndex = 0;
	FName Helmet;
	double NextPresetSwapTime = 0.0;

	TMap<FName, int32> UpgradeLevels;
	mutable TMap<int32, FCRSkillStats> StatCache;

	int32 Gold = 0;
	float DisplayedGold = 0.f;

	float PassiveTimers[FCRLoadoutPreset::PassiveCount] = {};
	float PassiveDurations[FCRLoadoutPreset::PassiveCount] = {};

	// 오비탈 칼날 (패시브 칼)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OrbitBlades;
	float OrbitAngle = 0.f;
	TMap<TWeakObjectPtr<AActor>, double> OrbitHitTimes;

	FDelegateHandle TuningChangedHandle;
	FDelegateHandle DataChangedHandle;
};
