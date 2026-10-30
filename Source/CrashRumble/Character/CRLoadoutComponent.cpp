#include "Character/CRLoadoutComponent.h"

#include "Character/CRCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRGameData.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GAS/CRCombat.h"
#include "Weapon/CRWeaponComponent.h"

namespace
{
	const TCHAR* BladeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FLinearColor BladeColor(0.85f, 0.9f, 1.f);
	constexpr int32 MaxOrbitBlades = 12;
	constexpr float OrbitHeight = -10.f;    // 캡슐 가운데 기준
	constexpr float BladeHitRadius = 35.f;  // 칼날 크기 1일 때
	constexpr float PassiveOriginHeight = 70.f;
	constexpr float PassiveRetry = 0.2f;    // 대상이 없으면 이만큼 뒤 다시
	constexpr float FlameTrailMinSpeed = 50.f;
	constexpr float SkyStrikeHeight = 900.f;
	const FName DefaultHelmet(TEXT("helmet.cap"));

	int32 StatKey(ECRWeaponType Family, ECRSkillForm Form)
	{
		return static_cast<int32>(Family) * 2 + static_cast<int32>(Form);
	}

	bool IsHelmetItem(FName Item)
	{
		return Item.ToString().StartsWith(TEXT("helmet."));
	}
}

UCRLoadoutComponent::UCRLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Inventory.SetNum(InventorySize);
}

void UCRLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		TuningChangedHandle = Tuning->OnChanged.AddWeakLambda(this, [this]() { InvalidateStats(); });
	}
	if (UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(this))
	{
		DataChangedHandle = Data->OnChanged.AddWeakLambda(this, [this]() { InvalidateStats(); });
	}
}

void UCRLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		Tuning->OnChanged.Remove(TuningChangedHandle);
	}
	if (UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(this))
	{
		Data->OnChanged.Remove(DataChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

ACRCharacter* UCRLoadoutComponent::GetSnowman() const
{
	return Cast<ACRCharacter>(GetOwner());
}

void UCRLoadoutComponent::GiveStarterItems()
{
	Inventory.Init(NAME_None, InventorySize);
	for (FCRLoadoutPreset& Preset : Presets)
	{
		Preset = FCRLoadoutPreset();
	}
	PresetIndex = 0;

	int32 Next = 0;
	for (const ECRWeaponType Family : CRSkill::GetSkillFamilies())
	{
		Inventory[Next++] = CRSkill::MakeSkillItem(Family, ECRSkillForm::Active);
	}
	for (const ECRWeaponType Family : CRSkill::GetSkillFamilies())
	{
		Inventory[Next++] = CRSkill::MakeSkillItem(Family, ECRSkillForm::Passive);
	}
	Helmet = DefaultHelmet;
	if (const UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(this))
	{
		for (const FCRHelmetDef& Def : Data->GetHelmets())
		{
			if (Def.Id != Helmet && Next < InventorySize)
			{
				Inventory[Next++] = Def.Id;
			}
		}
	}
	// 프리셋 1 좌클릭 = 칼 (인벤토리에서 꺼냄)
	MoveItem({ ECRSlotKind::Inventory, 0 }, { ECRSlotKind::Active, 0 });
	NotifyEquipmentChanged();
}

FName* UCRLoadoutComponent::FindSlot(const FCRSlotRef& Slot)
{
	FCRLoadoutPreset& Preset = Presets[PresetIndex];
	switch (Slot.Kind)
	{
	case ECRSlotKind::Inventory: return Inventory.IsValidIndex(Slot.Index) ? &Inventory[Slot.Index] : nullptr;
	case ECRSlotKind::Active: return Slot.Index >= 0 && Slot.Index < FCRLoadoutPreset::ActiveCount ? &Preset.Active[Slot.Index] : nullptr;
	case ECRSlotKind::Passive: return Slot.Index >= 0 && Slot.Index < FCRLoadoutPreset::PassiveCount ? &Preset.Passive[Slot.Index] : nullptr;
	case ECRSlotKind::Helmet: return Slot.Index == 0 ? &Helmet : nullptr;
	}
	return nullptr;
}

const FName* UCRLoadoutComponent::FindSlot(const FCRSlotRef& Slot) const
{
	return const_cast<UCRLoadoutComponent*>(this)->FindSlot(Slot);
}

FName UCRLoadoutComponent::GetSlotItem(const FCRSlotRef& Slot) const
{
	const FName* Item = FindSlot(Slot);
	return Item ? *Item : NAME_None;
}

bool UCRLoadoutComponent::CanPlace(FName Item, const FCRSlotRef& Slot) const
{
	if (Item.IsNone() || Slot.Kind == ECRSlotKind::Inventory)
	{
		return FindSlot(Slot) != nullptr;
	}
	ECRWeaponType Family;
	ECRSkillForm Form;
	switch (Slot.Kind)
	{
	case ECRSlotKind::Active: return CRSkill::ParseSkillItem(Item, Family, Form) && Form == ECRSkillForm::Active;
	case ECRSlotKind::Passive: return CRSkill::ParseSkillItem(Item, Family, Form) && Form == ECRSkillForm::Passive;
	case ECRSlotKind::Helmet: return IsHelmetItem(Item);
	default: return false;
	}
}

bool UCRLoadoutComponent::MoveItem(const FCRSlotRef& From, const FCRSlotRef& To)
{
	FName* Source = FindSlot(From);
	FName* Dest = FindSlot(To);
	if (!Source || !Dest || Source == Dest || Source->IsNone() || !CanPlace(*Source, To) || !CanPlace(*Dest, From))
	{
		return false;
	}
	Swap(*Source, *Dest);
	EnsureHelmet();
	if (From.Kind != ECRSlotKind::Inventory || To.Kind != ECRSlotKind::Inventory)
	{
		NotifyEquipmentChanged();
	}
	return true;
}

void UCRLoadoutComponent::EnsureHelmet()
{
	// 투구 칸은 비워 둘 수 없다: 기본 모자를 (인벤토리에 있으면 꺼내서) 쓴다
	if (Helmet.IsNone())
	{
		const int32 Index = Inventory.IndexOfByKey(DefaultHelmet);
		if (Index != INDEX_NONE)
		{
			Inventory[Index] = NAME_None;
		}
		Helmet = DefaultHelmet;
	}
}

int32 UCRLoadoutComponent::FindEmptyInventory() const
{
	return Inventory.IndexOfByPredicate([](FName Item) { return Item.IsNone(); });
}

bool UCRLoadoutComponent::QuickMove(const FCRSlotRef& From)
{
	const FName Item = GetSlotItem(From);
	if (Item.IsNone())
	{
		return false;
	}
	if (From.Kind != ECRSlotKind::Inventory)
	{
		const int32 Empty = FindEmptyInventory();
		if (From.Kind == ECRSlotKind::Helmet && Item == DefaultHelmet)
		{
			return false;
		}
		if (Empty == INDEX_NONE)
		{
			return false;
		}
		if (From.Kind == ECRSlotKind::Helmet)
		{
			// 투구를 벗으면 기본 모자
			Inventory[Empty] = Item;
			Helmet = NAME_None;
			EnsureHelmet();
			NotifyEquipmentChanged();
			return true;
		}
		return MoveItem(From, { ECRSlotKind::Inventory, Empty });
	}

	ECRWeaponType Family;
	ECRSkillForm Form;
	if (IsHelmetItem(Item))
	{
		// 지금 투구(기본 모자 제외)는 인벤토리로
		const FName Old = Helmet;
		Helmet = Item;
		Inventory[From.Index] = Old == DefaultHelmet ? NAME_None : Old;
		NotifyEquipmentChanged();
		return true;
	}
	if (!CRSkill::ParseSkillItem(Item, Family, Form))
	{
		return false;
	}
	const ECRSlotKind Kind = Form == ECRSkillForm::Active ? ECRSlotKind::Active : ECRSlotKind::Passive;
	const int32 Count = Form == ECRSkillForm::Active ? FCRLoadoutPreset::ActiveCount : FCRLoadoutPreset::PassiveCount;
	for (int32 i = 0; i < Count; ++i)
	{
		if (GetSlotItem({ Kind, i }).IsNone())
		{
			return MoveItem(From, { Kind, i });
		}
	}
	return MoveItem(From, { Kind, 0 });
}

void UCRLoadoutComponent::QuickEquipActive(ECRWeaponType Family)
{
	const FName Item = CRSkill::MakeSkillItem(Family, ECRSkillForm::Active);
	FCRLoadoutPreset& Preset = Presets[PresetIndex];
	if (Preset.Active[0] == Item)
	{
		return;
	}
	// 인벤토리나 다른 칸에 있으면 그 자리와 바꾸고, 없으면 새로 만든다 (훈련장 단축키)
	const int32 InInventory = Inventory.IndexOfByKey(Item);
	if (InInventory != INDEX_NONE)
	{
		MoveItem({ ECRSlotKind::Inventory, InInventory }, { ECRSlotKind::Active, 0 });
		return;
	}
	if (Preset.Active[1] == Item)
	{
		Swap(Preset.Active[0], Preset.Active[1]);
	}
	else
	{
		const FName Old = Preset.Active[0];
		Preset.Active[0] = Item;
		const int32 Empty = FindEmptyInventory();
		if (!Old.IsNone() && Empty != INDEX_NONE)
		{
			Inventory[Empty] = Old;
		}
	}
	NotifyEquipmentChanged();
}

ECRWeaponType UCRLoadoutComponent::GetActiveFamily(int32 Slot) const
{
	ECRWeaponType Family = ECRWeaponType::None;
	ECRSkillForm Form;
	if (Slot >= 0 && Slot < FCRLoadoutPreset::ActiveCount)
	{
		CRSkill::ParseSkillItem(Presets[PresetIndex].Active[Slot], Family, Form);
	}
	return Family;
}

ECRWeaponType UCRLoadoutComponent::GetPassiveFamily(int32 Slot) const
{
	ECRWeaponType Family = ECRWeaponType::None;
	ECRSkillForm Form;
	if (Slot >= 0 && Slot < FCRLoadoutPreset::PassiveCount)
	{
		CRSkill::ParseSkillItem(Presets[PresetIndex].Passive[Slot], Family, Form);
	}
	return Family;
}

bool UCRLoadoutComponent::SwapPreset()
{
	const double Now = FPlatformTime::Seconds();
	if (Now < NextPresetSwapTime)
	{
		return false;
	}
	NextPresetSwapTime = Now + UCRTuningSubsystem::GetSettings(this).PresetSwapCooldown;
	PresetIndex = (PresetIndex + 1) % PresetCount;
	for (int32 i = 0; i < FCRLoadoutPreset::PassiveCount; ++i)
	{
		PassiveTimers[i] = 0.f;
	}
	NotifyEquipmentChanged();
	return true;
}

float UCRLoadoutComponent::GetPassiveCooldownRatio(int32 Slot) const
{
	if (Slot < 0 || Slot >= FCRLoadoutPreset::PassiveCount || PassiveDurations[Slot] <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(PassiveTimers[Slot] / PassiveDurations[Slot], 0.f, 1.f);
}

void UCRLoadoutComponent::NotifyEquipmentChanged()
{
	OnEquipmentChanged.Broadcast();
}

FCRSkillStats UCRLoadoutComponent::GetStats(ECRWeaponType Family, ECRSkillForm Form) const
{
	const int32 Key = StatKey(Family, Form);
	if (const FCRSkillStats* Cached = StatCache.Find(Key))
	{
		return *Cached;
	}
	FCRSkillStats Stats = CRSkill::MakeBaseStats(UCRTuningSubsystem::GetSettings(this), Family, Form);
	if (const UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(this))
	{
		float Add[CRSkill::NumStats] = {};
		float Mul[CRSkill::NumStats];
		for (float& Value : Mul)
		{
			Value = 1.f;
		}
		for (const TPair<FName, int32>& Pair : UpgradeLevels)
		{
			const FCRUpgradeDef* Def = Data->FindUpgrade(Pair.Key);
			if (!Def || Def->Family != Family || Def->Form != Form || Pair.Value <= 0)
			{
				continue;
			}
			for (const FCRStatMod& Mod : Def->Mods)
			{
				const int32 Index = static_cast<int32>(Mod.Stat);
				Add[Index] += Mod.Add * Pair.Value;
				Mul[Index] *= FMath::Pow(Mod.Mul, static_cast<float>(Pair.Value));
			}
		}
		for (int32 i = 0; i < CRSkill::NumStats; ++i)
		{
			Stats.Values[i] = (Stats.Values[i] + Add[i]) * Mul[i];
		}
	}
	StatCache.Add(Key, Stats);
	return Stats;
}

int32 UCRLoadoutComponent::GetUpgradeLevel(FName Id) const
{
	const int32* Level = UpgradeLevels.Find(Id);
	return Level ? *Level : 0;
}

bool UCRLoadoutComponent::SetUpgradeLevel(FName Id, int32 Level, FString* OutMessage)
{
	const UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(this);
	const FCRUpgradeDef* Def = Data ? Data->FindUpgrade(Id) : nullptr;
	if (!Def)
	{
		if (OutMessage)
		{
			*OutMessage = FString::Printf(TEXT("업그레이드 %s 없음"), *Id.ToString());
		}
		return false;
	}
	Level = FMath::Clamp(Level, 0, Def->MaxLevel);
	if (Level > 0 && !Def->Requires.IsNone() && GetUpgradeLevel(Def->Requires) <= 0)
	{
		if (OutMessage)
		{
			const FCRUpgradeDef* Required = Data->FindUpgrade(Def->Requires);
			*OutMessage = FString::Printf(TEXT("%s: 먼저 '%s' 필요"), *Def->Name, Required ? *Required->Name : *Def->Requires.ToString());
		}
		return false;
	}
	if (GetUpgradeLevel(Id) == Level)
	{
		return false;
	}
	if (Level == 0)
	{
		UpgradeLevels.Remove(Id);
		// 이걸 선행 조건으로 쓰는 업그레이드도 내린다
		for (const FCRUpgradeDef& Other : Data->GetUpgrades())
		{
			if (Other.Requires == Id)
			{
				UpgradeLevels.Remove(Other.Id);
			}
		}
	}
	else
	{
		UpgradeLevels.Add(Id, Level);
	}
	if (OutMessage)
	{
		*OutMessage = FString::Printf(TEXT("%s %d / %d단계"), *Def->Name, Level, Def->MaxLevel);
	}
	InvalidateStats();
	NotifyEquipmentChanged();
	return true;
}

void UCRLoadoutComponent::ResetUpgrades()
{
	UpgradeLevels.Reset();
	InvalidateStats();
	NotifyEquipmentChanged();
}

void UCRLoadoutComponent::AddGold(int32 Amount)
{
	Gold = FMath::Max(Gold + Amount, 0);
}

void UCRLoadoutComponent::CopyEquipmentFrom(const UCRLoadoutComponent& Other)
{
	for (int32 i = 0; i < PresetCount; ++i)
	{
		Presets[i] = Other.Presets[i];
	}
	PresetIndex = Other.PresetIndex;
	Helmet = Other.Helmet;
	UpgradeLevels = Other.UpgradeLevels;
	InvalidateStats();
	NotifyEquipmentChanged();
}

void UCRLoadoutComponent::ClearEquipment()
{
	for (FCRLoadoutPreset& Preset : Presets)
	{
		Preset = FCRLoadoutPreset();
	}
	PresetIndex = 0;
	Helmet = NAME_None;
	UpgradeLevels.Reset();
	InvalidateStats();
	UpdateOrbit(0.f, nullptr);
	NotifyEquipmentChanged();
}

void UCRLoadoutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 골드 숫자: 차이가 클수록 빨리 드르륵 (최소 초당 20)
	if (DisplayedGold != Gold)
	{
		const float Speed = FMath::Max(FMath::Abs(Gold - DisplayedGold) * UCRTuningSubsystem::GetSettings(this).GoldRollSpeed, 20.f);
		DisplayedGold = FMath::FInterpConstantTo(DisplayedGold, static_cast<float>(Gold), DeltaTime, Speed);
	}
	UpdatePassives(DeltaTime);
}

void UCRLoadoutComponent::UpdatePassives(float DeltaTime)
{
	ACRCharacter* Snowman = GetSnowman();
	const bool bAlive = Snowman && !Snowman->IsDead() && !Snowman->IsInPool() && !Snowman->IsHidden();
	bool bOrbit = false;
	for (int32 i = 0; i < FCRLoadoutPreset::PassiveCount; ++i)
	{
		const ECRWeaponType Family = GetPassiveFamily(i);
		if (Family == ECRWeaponType::None || !bAlive)
		{
			continue;
		}
		const FCRSkillStats Stats = GetStats(Family, ECRSkillForm::Passive);
		if (Family == ECRWeaponType::Sword)
		{
			// 칼날 회전은 계속 돈다 (여러 칸이면 칼날이 늘어남)
			if (!bOrbit)
			{
				FCRSkillStats Combined = Stats;
				int32 Extra = 0;
				for (int32 j = i + 1; j < FCRLoadoutPreset::PassiveCount; ++j)
				{
					Extra += GetPassiveFamily(j) == ECRWeaponType::Sword ? Stats.GetInt(ECRStat::Count) : 0;
				}
				Combined[ECRStat::Count] = Stats.Get(ECRStat::Count) + Extra;
				UpdateOrbit(DeltaTime, &Combined);
				bOrbit = true;
			}
			continue;
		}
		PassiveTimers[i] -= DeltaTime;
		if (PassiveTimers[i] <= 0.f)
		{
			const float Interval = CRSkill::GetAttackInterval(Snowman, FMath::Max(Stats.Get(ECRStat::Interval), 0.05f));
			const bool bFired = FirePassive(Family, Stats);
			PassiveDurations[i] = bFired ? Interval : PassiveRetry;
			PassiveTimers[i] = PassiveDurations[i];
		}
	}
	if (!bOrbit)
	{
		UpdateOrbit(DeltaTime, nullptr);
	}
}

bool UCRLoadoutComponent::FirePassive(ECRWeaponType Family, const FCRSkillStats& Stats)
{
	ACRCharacter* Snowman = GetSnowman();
	UCRWeaponComponent* Weapon = Snowman ? Snowman->GetWeaponComponent() : nullptr;
	if (!Weapon)
	{
		return false;
	}
	const FVector Center = Snowman->GetActorLocation();
	const FVector Origin = Center + FVector(0.f, 0.f, PassiveOriginHeight);
	const float Range = Stats.Get(ECRStat::Range);

	if (Family == ECRWeaponType::Flame)
	{
		// 불 발자국: 움직이거나 적이 가까이 있으면 발밑에 불
		const bool bMoving = Snowman->GetVelocity().Size2D() > FlameTrailMinSpeed;
		if (!bMoving && !CRCombat::FindNearestHostile(Snowman, Center, Stats.Get(ECRStat::Radius) * 2.f))
		{
			return false;
		}
		Weapon->ShootFlame(Center - FVector(0.f, 0.f, 90.f), Snowman->GetActorForwardVector(), ECRSkillForm::Passive);
		return true;
	}

	ACRCharacter* Target = CRCombat::FindNearestHostile(Snowman, Center, Range);
	if (!Target)
	{
		return false;
	}
	const FVector TargetPoint = Target->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	switch (Family)
	{
	case ECRWeaponType::Laser:
		Weapon->ShootLaser(Origin, (TargetPoint - Origin).GetSafeNormal(), ECRSkillForm::Passive);
		return true;
	case ECRWeaponType::Projectile:
		Weapon->ShootProjectiles(Origin, (TargetPoint - Origin).GetSafeNormal(), ECRSkillForm::Passive);
		return true;
	case ECRWeaponType::Lightning:
		// 낙뢰: 하늘에서 대상에게
		Weapon->ShootLightning(TargetPoint + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), SkyStrikeHeight),
			TargetPoint, ECRSkillForm::Passive);
		return true;
	case ECRWeaponType::Missile:
		Weapon->ShootMissiles(Target->GetActorLocation() - FVector(0.f, 0.f, 90.f), ECRSkillForm::Passive);
		return true;
	default:
		return false;
	}
}

void UCRLoadoutComponent::UpdateOrbit(float DeltaTime, const FCRSkillStats* Stats)
{
	const int32 Count = Stats ? FMath::Clamp(Stats->GetInt(ECRStat::Count), 0, MaxOrbitBlades) : 0;
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// 칼날 메시는 처음 필요할 때 만들고 숨겨 두며 재사용
	while (OrbitBlades.Num() < Count)
	{
		UStaticMeshComponent* Blade = NewObject<UStaticMeshComponent>(Owner);
		Blade->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, BladeMesh));
		Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blade->SetCastShadow(false);
		Blade->SetUsingAbsoluteLocation(true);
		Blade->SetUsingAbsoluteRotation(true);
		Blade->SetUsingAbsoluteScale(true);
		Blade->SetupAttachment(Owner->GetRootComponent());
		Blade->RegisterComponent();
		if (UMaterialInterface* Material = UCRGameSettings::LoadGlowMaterial())
		{
			Blade->SetMaterial(0, Material);
			Blade->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(BladeColor.R, BladeColor.G, BladeColor.B, 1.f));
			Blade->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, 2.f);
		}
		OrbitBlades.Add(Blade);
	}
	for (int32 i = 0; i < OrbitBlades.Num(); ++i)
	{
		if (OrbitBlades[i])
		{
			OrbitBlades[i]->SetVisibility(i < Count);
		}
	}
	if (Count == 0 || !Stats)
	{
		OrbitHitTimes.Reset();
		return;
	}

	const float Radius = Stats->Get(ECRStat::Radius);
	const float Size = FMath::Max(Stats->Get(ECRStat::Size), 0.1f);
	OrbitAngle = FMath::Fmod(OrbitAngle + Stats->Get(ECRStat::Speed) * DeltaTime, 360.f);
	const FVector Center = Owner->GetActorLocation() + FVector(0.f, 0.f, OrbitHeight);
	const double Now = GetWorld()->GetTimeSeconds();
	const float HitInterval = FMath::Max(Stats->Get(ECRStat::Interval), 0.05f);
	ACRCharacter* Snowman = GetSnowman();

	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = OrbitAngle + 360.f * i / Count;
		const FVector Dir = FRotator(0.f, Angle, 0.f).Vector();
		const FVector Location = Center + Dir * Radius;
		// 칼날은 바깥을 향하고 진행 방향으로 살짝 눕힌다
		OrbitBlades[i]->SetWorldLocationAndRotation(Location, FRotator(0.f, Angle, 20.f));
		OrbitBlades[i]->SetWorldScale3D(FVector(0.7f, 0.08f, 0.02f) * Size);

		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		GetWorld()->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, Objects, FCollisionShape::MakeSphere(BladeHitRadius * Size),
			FCollisionQueryParams(SCENE_QUERY_STAT(CROrbitBlade), false, Owner));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			ACRCharacter* Victim = Cast<ACRCharacter>(Overlap.GetActor());
			if (!Victim || Victim->IsDead() || !CRCombat::IsHostile(Owner, Victim))
			{
				continue;
			}
			double& LastHit = OrbitHitTimes.FindOrAdd(Victim, -1000.0);
			if (Now - LastHit < HitInterval)
			{
				continue;
			}
			LastHit = Now;
			FHitResult Hit(Victim, Overlap.GetComponent(), Location, -Dir);
			Hit.bBlockingHit = true;
			CRCombat::ApplyWeaponHitFrom(Snowman, Victim, ECRWeaponType::Sword, Hit, Center, -1.f, ECRSkillForm::Passive);
		}
	}
	// 오래된 기록 정리
	if (OrbitHitTimes.Num() > 64)
	{
		for (auto It = OrbitHitTimes.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value() > HitInterval)
			{
				It.RemoveCurrent();
			}
		}
	}
}
