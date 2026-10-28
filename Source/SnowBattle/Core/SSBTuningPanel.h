#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USBTuningSubsystem;

/** Tab 옵션창. USBTuningSettings 레지스트리를 읽어 카테고리별 슬라이더/토글을 자동 생성한다. */
class SSBTuningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSBTuningPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USBTuningSubsystem>, Subsystem)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TWeakObjectPtr<USBTuningSubsystem> Subsystem;
	FSimpleDelegate OnCloseRequested;
};
