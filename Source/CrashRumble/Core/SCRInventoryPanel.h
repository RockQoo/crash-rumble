#pragma once

#include "CoreMinimal.h"
#include "Character/CRLoadoutComponent.h"
#include "Widgets/SLeafWidget.h"

/**
 * 인벤토리 (I). 아이콘 대신 네모 칸에 한글 이름. 위젯을 여러 개 만들지 않고 직접 그린다.
 * - 장착: 지금 프리셋의 액티브 2칸(좌클릭 / 우클릭) + 패시브 4칸, 투구 1칸 (투구는 프리셋 공용)
 * - 가방 6×4
 * - 끌어다 놓기로 옮기기 / 바꾸기, 우클릭으로 바로 장착·해제, 프리셋 탭 클릭 또는 X로 교체
 * 게임은 멈추지 않고, 키보드는 게임으로 간다 (포커스를 받지 않음).
 */
class SCRInventoryPanel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCRInventoryPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UCRLoadoutComponent>, Loadout)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }

private:
	struct FCell
	{
		FCRSlotRef Ref;
		FVector2f Position;
	};

	/** 모든 칸의 위치 (패널 기준). */
	const TArray<FCell>& GetCells() const;
	const FCell* FindCell(const FVector2f& Local) const;
	/** 프리셋 탭 (0 / 1), 없으면 INDEX_NONE. */
	int32 FindPresetTab(const FVector2f& Local) const;
	FString GetItemLabel(FName Item) const;
	FString GetItemDescription(FName Item) const;

	TWeakObjectPtr<UCRLoadoutComponent> Loadout;
	mutable TArray<FCell> Cells;

	bool bDragging = false;
	FCRSlotRef DragFrom;
	FVector2f MouseLocal = FVector2f::ZeroVector;
	bool bMouseInside = false;
};
