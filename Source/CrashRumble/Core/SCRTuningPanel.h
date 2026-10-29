#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UCRTuningSubsystem;

DECLARE_DELEGATE_OneParam(FCROnSpawnTargets, int32);
DECLARE_DELEGATE_OneParam(FCROnPreviewCategory, const FString&);

/**
 * Tab 옵션창. 위쪽은 다수 타격 테스트 버튼(몬스터 생성/제거),
 * 아래는 UCRTuningSettings 레지스트리를 읽어 카테고리별 슬라이더/토글을 자동 생성한다.
 */
class SCRTuningPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCRTuningPanel)
		: _ShowSpawnPage(true)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UCRTuningSubsystem>, Subsystem)
		/** 몬스터 · 팀전 탭 (타격 체크 모드에서만). */
		SLATE_ARGUMENT(bool, ShowSpawnPage)
		SLATE_EVENT(FSimpleDelegate, OnCloseRequested)
		SLATE_EVENT(FSimpleDelegate, OnBackToTitle)
		/** 탭의 "미리보기" 버튼: 그 효과를 플레이어 앞에 바로 터뜨린다. */
		SLATE_EVENT(FCROnPreviewCategory, OnPreview)
		SLATE_EVENT(FCROnSpawnTargets, OnSpawnTargets)
		SLATE_EVENT(FSimpleDelegate, OnClearTargets)
		SLATE_EVENT(FCROnSpawnTargets, OnStartBattle)
		/** "벤치마크" 탭의 시작 / 중지 버튼과 결과 글. */
		SLATE_EVENT(FSimpleDelegate, OnStartBenchmark)
		SLATE_EVENT(FSimpleDelegate, OnStopBenchmark)
		SLATE_ATTRIBUTE(FText, BenchmarkText)
		SLATE_ATTRIBUTE(FText, StatusText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** 다음에 열 때 보여줄 탭 (녹화/자동 확인용). */
	static void SetCurrentTab(int32 Index);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> MakeItemRow(const struct FCRTuningItem& Item, const FSlateFontInfo& Font);
	TSharedRef<SWidget> MakeSpawnPage(const FArguments& InArgs, const FSlateFontInfo& HeaderFont, const FSlateFontInfo& Font);
	TSharedRef<SWidget> MakeBenchmarkControls(const FArguments& InArgs, const FSlateFontInfo& Font);
	void ResetCategory(const FString& Category);

	TWeakObjectPtr<UCRTuningSubsystem> Subsystem;
	FSimpleDelegate OnCloseRequested;
	FSimpleDelegate OnBackToTitle;
	FCROnPreviewCategory OnPreview;
	FCROnSpawnTargets OnSpawnTargets;
	FSimpleDelegate OnClearTargets;
	FCROnSpawnTargets OnStartBattle;
	FSimpleDelegate OnStartBenchmark;
	FSimpleDelegate OnStopBenchmark;
};
