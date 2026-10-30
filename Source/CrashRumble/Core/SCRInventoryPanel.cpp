#include "Core/SCRInventoryPanel.h"

#include "Core/CRGameData.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float Pad = 16.f;
	constexpr float Cell = 72.f;
	constexpr float Gap = 6.f;
	constexpr float LabelWidth = 70.f;
	constexpr float HeaderHeight = 30.f;
	constexpr float TabHeight = 28.f;
	constexpr float TabWidth = 110.f;
	constexpr float SectionGap = 22.f;
	constexpr float InfoHeight = 54.f;

	const float PanelWidth = Pad * 2.f + UCRLoadoutComponent::InventoryColumns * Cell + (UCRLoadoutComponent::InventoryColumns - 1) * Gap;
	const float TabsY = Pad + HeaderHeight + 6.f;
	const float ActiveY = TabsY + TabHeight + SectionGap;
	const float PassiveY = ActiveY + Cell + SectionGap;
	const float BagY = PassiveY + Cell + SectionGap + 6.f;
	const float BagHeight = UCRLoadoutComponent::InventoryRows * Cell + (UCRLoadoutComponent::InventoryRows - 1) * Gap;
	const float InfoY = BagY + BagHeight + 12.f;
	const float PanelHeight = InfoY + InfoHeight + Pad;

	const FLinearColor PanelColor(0.02f, 0.025f, 0.035f, 0.9f);
	const FLinearColor CellColor(0.12f, 0.13f, 0.16f, 1.f);
	const FLinearColor ActiveColor(0.16f, 0.3f, 0.6f, 1.f);
	const FLinearColor PassiveColor(0.14f, 0.45f, 0.25f, 1.f);
	const FLinearColor HelmetColor(0.55f, 0.42f, 0.12f, 1.f);
	const FLinearColor TextColor(0.95f, 0.95f, 1.f, 1.f);
	const FLinearColor DimText(0.6f, 0.62f, 0.7f, 1.f);
	const FLinearColor GoldColor(1.f, 0.82f, 0.2f, 1.f);

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FVector2f& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), White(), ESlateDrawEffect::None, Color);
	}

	void Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position, const FString& String,
		const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2f(1000.f, 40.f), FSlateLayoutTransform(Position)), String, Font,
			ESlateDrawEffect::None, Color);
	}

	FVector2f Measure(const FString& String, const FSlateFontInfo& Font)
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return FVector2f(FontMeasure->Measure(String, Font));
	}

	FLinearColor SlotColor(ECRSlotKind Kind, FName Item)
	{
		const FString Name = Item.ToString();
		if (Name.StartsWith(TEXT("active.")))
		{
			return ActiveColor;
		}
		if (Name.StartsWith(TEXT("passive.")))
		{
			return PassiveColor;
		}
		if (Name.StartsWith(TEXT("helmet.")))
		{
			return HelmetColor;
		}
		return CellColor;
	}
}

void SCRInventoryPanel::Construct(const FArguments& InArgs)
{
	Loadout = InArgs._Loadout;
}

FVector2D SCRInventoryPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(PanelWidth, PanelHeight);
}

const TArray<SCRInventoryPanel::FCell>& SCRInventoryPanel::GetCells() const
{
	if (Cells.Num() == 0)
	{
		for (int32 i = 0; i < FCRLoadoutPreset::ActiveCount; ++i)
		{
			Cells.Add({ { ECRSlotKind::Active, i }, FVector2f(Pad + LabelWidth + i * (Cell + Gap), ActiveY) });
		}
		Cells.Add({ { ECRSlotKind::Helmet, 0 }, FVector2f(PanelWidth - Pad - Cell, ActiveY) });
		for (int32 i = 0; i < FCRLoadoutPreset::PassiveCount; ++i)
		{
			Cells.Add({ { ECRSlotKind::Passive, i }, FVector2f(Pad + LabelWidth + i * (Cell + Gap), PassiveY) });
		}
		for (int32 i = 0; i < UCRLoadoutComponent::InventorySize; ++i)
		{
			const int32 Column = i % UCRLoadoutComponent::InventoryColumns;
			const int32 Row = i / UCRLoadoutComponent::InventoryColumns;
			Cells.Add({ { ECRSlotKind::Inventory, i }, FVector2f(Pad + Column * (Cell + Gap), BagY + Row * (Cell + Gap)) });
		}
	}
	return Cells;
}

const SCRInventoryPanel::FCell* SCRInventoryPanel::FindCell(const FVector2f& Local) const
{
	for (const FCell& Entry : GetCells())
	{
		if (Local.X >= Entry.Position.X && Local.Y >= Entry.Position.Y && Local.X < Entry.Position.X + Cell && Local.Y < Entry.Position.Y + Cell)
		{
			return &Entry;
		}
	}
	return nullptr;
}

int32 SCRInventoryPanel::FindPresetTab(const FVector2f& Local) const
{
	for (int32 i = 0; i < UCRLoadoutComponent::PresetCount; ++i)
	{
		const float X = Pad + i * (TabWidth + Gap);
		if (Local.X >= X && Local.X < X + TabWidth && Local.Y >= TabsY && Local.Y < TabsY + TabHeight)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString SCRInventoryPanel::GetItemLabel(FName Item) const
{
	ECRWeaponType Family;
	ECRSkillForm Form;
	if (CRSkill::ParseSkillItem(Item, Family, Form))
	{
		return CRSkill::GetFamilyLabel(Family);
	}
	const UCRGameDataSubsystem* Data = Loadout.IsValid() ? UCRGameDataSubsystem::Get(Loadout.Get()) : nullptr;
	if (const FCRHelmetDef* Helmet = Data ? Data->FindHelmet(Item) : nullptr)
	{
		return Helmet->Name;
	}
	return Item.ToString();
}

FString SCRInventoryPanel::GetItemDescription(FName Item) const
{
	ECRWeaponType Family;
	ECRSkillForm Form;
	if (!CRSkill::ParseSkillItem(Item, Family, Form))
	{
		return Item.ToString().StartsWith(TEXT("helmet.")) ? FString::Printf(TEXT("%s · 투구 (지금은 외형만)"), *GetItemLabel(Item)) : FString();
	}
	FString Effect;
	if (Form == ECRSkillForm::Active)
	{
		Effect = TEXT("액티브 · 좌클릭 / 우클릭 칸에 끼우면 그 버튼으로 사용");
	}
	else
	{
		switch (Family)
		{
		case ECRWeaponType::Sword: Effect = TEXT("패시브 · 주변을 도는 칼날"); break;
		case ECRWeaponType::Laser: Effect = TEXT("패시브 · 가까운 적에게 자동 레이저"); break;
		case ECRWeaponType::Projectile: Effect = TEXT("패시브 · 가까운 적에게 자동 사격"); break;
		case ECRWeaponType::Lightning: Effect = TEXT("패시브 · 가까운 적에게 낙뢰"); break;
		case ECRWeaponType::Missile: Effect = TEXT("패시브 · 가까운 적에게 런처 한 발"); break;
		case ECRWeaponType::Flame: Effect = TEXT("패시브 · 움직이면 발밑에 불 장판"); break;
		default: break;
		}
	}
	// 이 스킬에 걸린 업그레이드 수
	int32 Upgrades = 0;
	if (const UCRGameDataSubsystem* Data = UCRGameDataSubsystem::Get(Loadout.Get()))
	{
		for (const FCRUpgradeDef& Def : Data->GetUpgrades())
		{
			Upgrades += Def.Family == Family && Def.Form == Form && Loadout->GetUpgradeLevel(Def.Id) > 0 ? 1 : 0;
		}
	}
	return FString::Printf(TEXT("%s · %s%s"), *GetItemLabel(Item), *Effect,
		Upgrades > 0 ? *FString::Printf(TEXT(" · 업그레이드 %d개"), Upgrades) : TEXT(""));
}

int32 SCRInventoryPanel::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const UCRLoadoutComponent* Owner = Loadout.Get();
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	const int32 BackLayer = LayerId;
	const int32 CellLayer = LayerId + 1;
	const int32 TextLayer = LayerId + 2;
	const int32 DragLayer = LayerId + 3;

	Box(Out, BackLayer, Geometry, FVector2f::ZeroVector, FVector2f(PanelWidth, PanelHeight), PanelColor);
	Text(Out, TextLayer, Geometry, FVector2f(Pad, Pad), TEXT("인벤토리"), TitleFont, TextColor);
	Text(Out, TextLayer, Geometry, FVector2f(Pad + 90.f, Pad + 5.f), TEXT("I 닫기 · 끌어서 옮기기 · 우클릭 바로 장착/해제"), SmallFont, DimText);
	if (!Owner)
	{
		return DragLayer;
	}
	const FString Gold = FString::Printf(TEXT("골드 %d"), Owner->GetDisplayedGold());
	Text(Out, TextLayer, Geometry, FVector2f(PanelWidth - Pad - Measure(Gold, TitleFont).X, Pad), Gold, TitleFont, GoldColor);

	// 프리셋 탭
	for (int32 i = 0; i < UCRLoadoutComponent::PresetCount; ++i)
	{
		const FVector2f Position(Pad + i * (TabWidth + Gap), TabsY);
		const bool bCurrent = i == Owner->GetPresetIndex();
		Box(Out, CellLayer, Geometry, Position, FVector2f(TabWidth, TabHeight), bCurrent ? FLinearColor(0.25f, 0.55f, 1.f) : FLinearColor(0.2f, 0.2f, 0.25f));
		Text(Out, TextLayer, Geometry, Position + FVector2f(22.f, 5.f), FString::Printf(TEXT("프리셋 %d"), i + 1), RowFont, TextColor);
	}
	Text(Out, TextLayer, Geometry, FVector2f(Pad + 2.f * (TabWidth + Gap) + 4.f, TabsY + 6.f), TEXT("X: 교체"), SmallFont, DimText);

	Text(Out, TextLayer, Geometry, FVector2f(Pad, ActiveY + Cell * 0.5f - 9.f), TEXT("액티브"), RowFont, FLinearColor(0.55f, 0.75f, 1.f));
	Text(Out, TextLayer, Geometry, FVector2f(Pad, PassiveY + Cell * 0.5f - 9.f), TEXT("패시브"), RowFont, FLinearColor(0.55f, 1.f, 0.65f));
	Text(Out, TextLayer, Geometry, FVector2f(PanelWidth - Pad - Cell - 40.f, ActiveY + Cell * 0.5f - 9.f), TEXT("투구"), RowFont, GoldColor);
	Text(Out, TextLayer, Geometry, FVector2f(Pad, BagY - 20.f), TEXT("가방"), RowFont, DimText);

	const FCell* Hovered = bMouseInside ? FindCell(MouseLocal) : nullptr;
	const FName DragItem = bDragging ? Owner->GetSlotItem(DragFrom) : NAME_None;
	for (const FCell& Entry : GetCells())
	{
		const FName Item = Owner->GetSlotItem(Entry.Ref);
		const bool bHovered = Hovered && Hovered->Ref == Entry.Ref;
		const bool bSource = bDragging && DragFrom == Entry.Ref;
		// 끌고 있을 때: 놓을 수 있는 칸은 밝게, 못 놓는 칸은 빨갛게
		FLinearColor Border(0.3f, 0.32f, 0.38f);
		if (bDragging && bHovered)
		{
			const bool bOk = Owner->CanPlace(DragItem, Entry.Ref) && Owner->CanPlace(Item, DragFrom);
			Border = bOk ? FLinearColor(1.f, 0.9f, 0.4f) : FLinearColor(1.f, 0.2f, 0.15f);
		}
		else if (bHovered)
		{
			Border = FLinearColor(0.85f, 0.85f, 0.95f);
		}
		Box(Out, CellLayer, Geometry, Entry.Position - FVector2f(2.f, 2.f), FVector2f(Cell + 4.f, Cell + 4.f), Border);
		const FLinearColor Fill = Item.IsNone() || bSource ? CellColor : SlotColor(Entry.Ref.Kind, Item);
		Box(Out, CellLayer, Geometry, Entry.Position, FVector2f(Cell, Cell), Fill);

		if (Item.IsNone() || bSource)
		{
			// 빈 칸 안내
			const TCHAR* Hint = Entry.Ref.Kind == ECRSlotKind::Active ? (Entry.Ref.Index == 0 ? TEXT("좌클릭") : TEXT("우클릭·펀치")) : nullptr;
			if (Hint)
			{
				Text(Out, TextLayer, Geometry, Entry.Position + FVector2f(8.f, Cell * 0.5f - 12.f), Hint, SmallFont, DimText);
			}
			continue;
		}
		const FString Label = GetItemLabel(Item);
		const FVector2f Size = Measure(Label, NameFont);
		Text(Out, TextLayer, Geometry, Entry.Position + FVector2f((Cell - Size.X) * 0.5f, (Cell - Size.Y) * 0.5f + 4.f), Label, NameFont, TextColor);
		const FString KindTag = Item.ToString().StartsWith(TEXT("active.")) ? TEXT("액티브") : Item.ToString().StartsWith(TEXT("passive.")) ? TEXT("패시브") : TEXT("투구");
		Text(Out, TextLayer, Geometry, Entry.Position + FVector2f(5.f, 3.f), KindTag, SmallFont, FLinearColor(1.f, 1.f, 1.f, 0.6f));
	}

	// 설명 줄: 끌고 있는 아이템 > 마우스 아래 아이템
	const FName InfoItem = bDragging ? DragItem : Hovered ? Owner->GetSlotItem(Hovered->Ref) : NAME_None;
	Box(Out, CellLayer, Geometry, FVector2f(Pad, InfoY), FVector2f(PanelWidth - Pad * 2.f, InfoHeight), FLinearColor(0.07f, 0.08f, 0.12f, 1.f));
	const FString Info = InfoItem.IsNone() ? TEXT("칸에 마우스를 올리면 설명. 좌클릭 칸 = 공격 버튼, 우클릭 칸이 비면 펀치, 패시브는 자동 발동.") : GetItemDescription(InfoItem);
	Text(Out, TextLayer, Geometry, FVector2f(Pad + 8.f, InfoY + 8.f), Info, RowFont, InfoItem.IsNone() ? DimText : TextColor);

	// 끌고 있는 아이템은 커서를 따라간다
	if (bDragging && !DragItem.IsNone())
	{
		const FVector2f Position = MouseLocal - FVector2f(Cell * 0.5f, Cell * 0.5f);
		FLinearColor Fill = SlotColor(DragFrom.Kind, DragItem);
		Fill.A = 0.85f;
		Box(Out, DragLayer, Geometry, Position, FVector2f(Cell, Cell), Fill);
		const FString Label = GetItemLabel(DragItem);
		const FVector2f Size = Measure(Label, NameFont);
		Text(Out, DragLayer + 1, Geometry, Position + FVector2f((Cell - Size.X) * 0.5f, (Cell - Size.Y) * 0.5f), Label, NameFont, TextColor);
	}
	return DragLayer + 1;
}

FReply SCRInventoryPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UCRLoadoutComponent* Owner = Loadout.Get();
	MouseLocal = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (!Owner)
	{
		return FReply::Handled();
	}
	if (const int32 Tab = FindPresetTab(MouseLocal); Tab != INDEX_NONE)
	{
		if (Tab != Owner->GetPresetIndex())
		{
			Owner->SwapPreset();
		}
		return FReply::Handled();
	}
	const FCell* Hit = FindCell(MouseLocal);
	if (!Hit || Owner->GetSlotItem(Hit->Ref).IsNone())
	{
		return FReply::Handled(); // 패널 위 클릭은 게임(공격)으로 넘기지 않는다
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Owner->QuickMove(Hit->Ref);
		return FReply::Handled();
	}
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		DragFrom = Hit->Ref;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return FReply::Handled();
}

FReply SCRInventoryPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bDragging || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	bDragging = false;
	MouseLocal = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	if (UCRLoadoutComponent* Owner = Loadout.Get())
	{
		if (const FCell* Target = FindCell(MouseLocal); Target && !(Target->Ref == DragFrom))
		{
			Owner->MoveItem(DragFrom, Target->Ref);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SCRInventoryPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MouseLocal = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	bMouseInside = true;
	return FReply::Handled();
}

void SCRInventoryPanel::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bMouseInside = false;
	SLeafWidget::OnMouseLeave(MouseEvent);
}
