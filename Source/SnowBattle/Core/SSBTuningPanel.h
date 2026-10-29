#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USBTuningSubsystem;

DECLARE_DELEGATE_OneParam(FSBOnSpawnTargets, int32);

/**
 * Tab 옵션창. 위쪽은 다수 타격 테스트 버튼(몬스터 생성/제거),
 * 아래는 USBTuningSettings 레지스트리를 읽어 카테고리별 슬라이더/토글을 자동 생성한다.
 */
class SSBTuningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSBTuningPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USBTuningSubsystem>, Subsystem)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
		SLATE_EVENT(FSBOnSpawnTargets, OnSpawnTargets)
		SLATE_EVENT(FSimpleDelegate, OnClearTargets)
		SLATE_EVENT(FSBOnSpawnTargets, OnStartBattle)
		SLATE_ATTRIBUTE(FText, StatusText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeItemRow(const struct FSBTuningItem& Item, const FSlateFontInfo& Font);
	TSharedRef<SWidget> MakeSpawnPage(const FArguments& InArgs, const FSlateFontInfo& HeaderFont, const FSlateFontInfo& Font);
	void ResetCategory(const FString& Category);

	TWeakObjectPtr<USBTuningSubsystem> Subsystem;
	FSimpleDelegate OnCloseRequested;
	FSBOnSpawnTargets OnSpawnTargets;
	FSimpleDelegate OnClearTargets;
	FSBOnSpawnTargets OnStartBattle;
};
