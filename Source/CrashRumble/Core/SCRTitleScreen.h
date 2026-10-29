#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ECRMode : uint8;

DECLARE_DELEGATE_OneParam(FCROnModeSelected, ECRMode);

/** 타이틀 메뉴: 타격 체크 / 맵 제작 / 게임 시작 / 종료. 숫자키 1~3으로도 고른다. */
class SCRTitleScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCRTitleScreen) {}
		SLATE_EVENT(FCROnModeSelected, OnModeSelected)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeModeButton(ECRMode Mode, int32 Number, const FText& Description);
	void Select(ECRMode Mode);

	FCROnModeSelected OnModeSelected;
	FSimpleDelegate OnQuit;
	bool bSelected = false;
};
