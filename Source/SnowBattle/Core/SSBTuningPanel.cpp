#include "Core/SSBTuningPanel.h"

#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSBTuningPanel::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	OnCloseRequested = InArgs._OnCloseRequested;

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	FString LastCategory;

	for (const FSBTuningItem& Item : USBTuningSettings::GetItems())
	{
		const FString Category = Item.Category.ToString();
		if (Category != LastCategory)
		{
			LastCategory = Category;
			Rows->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(Item.Category).Font(HeaderFont).ColorAndOpacity(FLinearColor(0.55f, 0.8f, 1.f))
			];
		}

		const FName Name = Item.GetName();
		TSharedPtr<SWidget> Editor;
		if (Item.IsBool())
		{
			Editor = SNew(SCheckBox)
				.IsChecked_Lambda([this, Name]()
				{
					const FSBTuningItem* Found = USBTuningSettings::FindItem(Name);
					return Subsystem.IsValid() && Found && Found->GetValue(Subsystem->GetMutableSettings()) > 0.5f
						? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this, Name](ECheckBoxState State)
				{
					if (Subsystem.IsValid())
					{
						Subsystem->SetValue(Name, State == ECheckBoxState::Checked ? 1.f : 0.f);
					}
				});
		}
		else
		{
			Editor = SNew(SSpinBox<float>)
				.MinValue(Item.Min).MaxValue(Item.Max)
				.MinSliderValue(Item.Min).MaxSliderValue(Item.Max)
				.Delta(Item.Step)
				.Font(RowFont)
				.Value_Lambda([this, Name]()
				{
					const FSBTuningItem* Found = USBTuningSettings::FindItem(Name);
					return Subsystem.IsValid() && Found ? Found->GetValue(Subsystem->GetMutableSettings()) : 0.f;
				})
				.OnValueChanged_Lambda([this, Name](float NewValue)
				{
					if (Subsystem.IsValid())
					{
						Subsystem->SetValue(Name, NewValue);
					}
				});
		}

		Rows->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Item.Label).Font(RowFont)
			]
			+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
			[
				Editor.ToSharedRef()
			]
		];
	}

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(24.f)
	[
		SNew(SBox)
		.WidthOverride(460.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.92f))
			.Padding(16.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("튜닝 옵션"))).Font(TitleFont)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Tab: 닫기 · 값은 자동 저장")))
					.Font(RowFont)
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f))
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[ Rows ]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("기본값으로 초기화")))
						.OnClicked_Lambda([this]()
						{
							if (Subsystem.IsValid())
							{
								Subsystem->ResetToDefaults();
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("닫기")))
						.OnClicked_Lambda([this]()
						{
							OnCloseRequested.ExecuteIfBound();
							return FReply::Handled();
						})
					]
				]
			]
		]
	];
}

FReply SSBTuningPanel::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnCloseRequested.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
