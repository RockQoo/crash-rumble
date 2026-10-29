#include "Core/SCRTitleScreen.h"

#include "Core/CRModes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor ButtonColor(0.12f, 0.2f, 0.45f, 0.95f);
	const FLinearColor AccentColor(1.f, 0.55f, 0.25f);
	const FLinearColor DimTextColor(0.7f, 0.72f, 0.8f);
}

void SCRTitleScreen::Construct(const FArguments& InArgs)
{
	OnModeSelected = InArgs._OnModeSelected;
	OnQuit = InArgs._OnQuit;

	const FSlateFontInfo LogoFont = FCoreStyle::GetDefaultFontStyle("Bold", 50);
	const FSlateFontInfo SubFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.78f))
		.Padding(FMargin(56.f, 48.f))
		[
			SNew(SBox)
			.WidthOverride(560.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("CRASH RUMBLE"))).Font(LogoFont).ColorAndOpacity(AccentColor)
					.ShadowOffset(FVector2D(2.f, 2.f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f, 0.f, 0.f, 36.f)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("타격감 배틀 프로토타입"))).Font(SubFont).ColorAndOpacity(DimTextColor)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					MakeModeButton(ECRMode::HitCheck, 1, FText::FromString(TEXT("무기 3종 · 타격감 · 몬스터 / 팀전 샌드박스")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					MakeModeButton(ECRMode::MapEdit, 2, FText::FromString(TEXT("자유 카메라로 맵 둘러보기 (블럭·맵 파일은 11단계)")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					MakeModeButton(ECRMode::InGame, 3, FText::FromString(TEXT("인게임 틀 (내용은 타격 테스트 뒤 결정)")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f).HAlign(HAlign_Left)
				[
					SNew(SButton)
					.ButtonColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.3f, 0.9f))
					.ContentPadding(FMargin(22.f, 6.f))
					.OnClicked_Lambda([this]()
					{
						OnQuit.ExecuteIfBound();
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("종료"))).Font(SubFont)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 28.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("숫자키 1 / 2 / 3으로도 선택\n각 모드에서 Esc 또는 Tab 옵션창의 '타이틀로'로 돌아온다")))
					.Font(SmallFont)
					.ColorAndOpacity(FLinearColor(0.55f, 0.57f, 0.65f))
				]
			]
		]
	];
}

TSharedRef<SWidget> SCRTitleScreen::MakeModeButton(ECRMode Mode, int32 Number, const FText& Description)
{
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo DescFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	const FText Name = Mode == ECRMode::InGame ? FText::FromString(TEXT("게임 시작")) : CRModes::GetDisplayName(Mode);

	return SNew(SButton)
		.ButtonColorAndOpacity(ButtonColor)
		.ContentPadding(FMargin(18.f, 10.f))
		.OnClicked_Lambda([this, Mode]()
		{
			Select(Mode);
			return FReply::Handled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
			[
				SNew(STextBlock).Text(FText::AsNumber(Number)).Font(NameFont).ColorAndOpacity(AccentColor)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Name).Font(NameFont)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Description).Font(DescFont).ColorAndOpacity(DimTextColor).AutoWrapText(true)
				]
			]
		];
}

void SCRTitleScreen::Select(ECRMode Mode)
{
	// 맵을 여는 동안 중복 클릭 방지
	if (!bSelected)
	{
		bSelected = true;
		OnModeSelected.ExecuteIfBound(Mode);
	}
}

FReply SCRTitleScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::One || Key == EKeys::NumPadOne)
	{
		Select(ECRMode::HitCheck);
	}
	else if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
	{
		Select(ECRMode::MapEdit);
	}
	else if (Key == EKeys::Three || Key == EKeys::NumPadThree)
	{
		Select(ECRMode::InGame);
	}
	else
	{
		return FReply::Unhandled();
	}
	return FReply::Handled();
}
