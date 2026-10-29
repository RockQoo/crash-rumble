#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UCRTuningSubsystem;

DECLARE_DELEGATE_OneParam(FCROnSpawnTargets, int32);

/**
 * Tab 옵션창. 위쪽은 다수 타격 테스트 버튼(몬스터 생성/제거),
 * 아래는 UCRTuningSettings 레지스트리를 읽어 카테고리별 슬라이더/토글을 자동 생성한다.
 */
class SCRTuningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCRTuningPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UCRTuningSubsystem>, Subsystem)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
		SLATE_EVENT(FCROnSpawnTargets, OnSpawnTargets)
		SLATE_EVENT(FSimpleDelegate, OnClearTargets)
		SLATE_EVENT(FCROnSpawnTargets, OnStartBattle)
		SLATE_ATTRIBUTE(FText, StatusText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeItemRow(const struct FCRTuningItem& Item, const FSlateFontInfo& Font);
	TSharedRef<SWidget> MakeSpawnPage(const FArguments& InArgs, const FSlateFontInfo& HeaderFont, const FSlateFontInfo& Font);
	void ResetCategory(const FString& Category);

	TWeakObjectPtr<UCRTuningSubsystem> Subsystem;
	FSimpleDelegate OnCloseRequested;
	FCROnSpawnTargets OnSpawnTargets;
	FSimpleDelegate OnClearTargets;
	FCROnSpawnTargets OnStartBattle;
};
