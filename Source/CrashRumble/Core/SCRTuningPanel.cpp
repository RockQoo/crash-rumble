#include "Core/SCRTuningPanel.h"

#include "Character/CRLoadoutComponent.h"
#include "Core/CRGameData.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// 창을 닫았다 열어도 보던 탭 유지
	int32 GLastTuningTab = 0;

	const FLinearColor TabSelectedColor(0.25f, 0.55f, 1.f);
	const FLinearColor TabNormalColor(0.35f, 0.35f, 0.4f);
}

void SCRTuningPanel::SetCurrentTab(int32 Index)
{
	GLastTuningTab = FMath::Max(Index, 0);
}

TSharedRef<SWidget> SCRTuningPanel::MakeItemRow(const FCRTuningItem& Item, const FSlateFontInfo& Font)
{
	const FName Name = Item.GetName();
	TSharedPtr<SWidget> Editor;
	if (Item.IsBool())
	{
		Editor = SNew(SCheckBox)
			.IsChecked_Lambda([this, Name]()
			{
				const FCRTuningItem* Found = UCRTuningSettings::FindItem(Name);
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
			.Font(Font)
			.Value_Lambda([this, Name]()
			{
				const FCRTuningItem* Found = UCRTuningSettings::FindItem(Name);
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

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(Item.Label).Font(Font).AutoWrapText(true)
		]
		+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
		[
			Editor.ToSharedRef()
		];
}

TSharedRef<SWidget> SCRTuningPanel::MakeSpawnPage(const FArguments& InArgs, const FSlateFontInfo& HeaderFont, const FSlateFontInfo& Font)
{
	TSharedRef<SVerticalBox> Page = SNew(SVerticalBox);
	Page->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock).Text(InArgs._StatusText).Font(Font).ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.8f))
	];

	Page->AddSlot().AutoHeight().Padding(0.f, 4.f)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("몬스터 생성 (풀링)"))).Font(HeaderFont).ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.8f))
	];
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	for (const int32 Count : { 10, 50, 100, 500 })
	{
		Buttons->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(FString::Printf(TEXT("+%d"), Count)))
			.OnClicked_Lambda([this, Count]()
			{
				OnSpawnTargets.ExecuteIfBound(Count);
				return FReply::Handled();
			})
		];
	}
	Buttons->AddSlot().FillWidth(1.4f)
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.Text(FText::FromString(TEXT("모두 제거")))
		.OnClicked_Lambda([this]()
		{
			OnClearTargets.ExecuteIfBound();
			return FReply::Handled();
		})
	];
	Page->AddSlot().AutoHeight()[ Buttons ];

	// 팀 전투: 기존 몬스터를 정리하고 레드/블루 진영 생성
	Page->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("팀 전투 (레드 vs 블루)"))).Font(HeaderFont).ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.8f))
	];
	TSharedRef<SHorizontalBox> BattleButtons = SNew(SHorizontalBox);
	for (const int32 PerTeam : { 10, 25, 50, 100 })
	{
		BattleButtons->AddSlot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(FString::Printf(TEXT("%d vs %d"), PerTeam, PerTeam)))
			.OnClicked_Lambda([this, PerTeam]()
			{
				OnStartBattle.ExecuteIfBound(PerTeam);
				return FReply::Handled();
			})
		];
	}
	Page->AddSlot().AutoHeight()[ BattleButtons ];

	// 인게임 웨이브를 훈련장에서 시험 (북쪽에서 칼 / 총 / 런처 몬스터가 계속 몰려옴)
	Page->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("인게임 웨이브 (북쪽에서 계속)"))).Font(HeaderFont).ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.8f))
	];
	Page->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(TEXT("웨이브 시작")))
			.OnClicked_Lambda([this]()
			{
				OnStartWave.ExecuteIfBound();
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(TEXT("웨이브 중지")))
			.OnClicked_Lambda([this]()
			{
				OnStopWave.ExecuteIfBound();
				return FReply::Handled();
			})
		]
	];
	return Page;
}

TSharedRef<SWidget> SCRTuningPanel::MakeUpgradePage(const FSlateFontInfo& HeaderFont, const FSlateFontInfo& Font)
{
	TSharedRef<SVerticalBox> Page = SNew(SVerticalBox);
	Page->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("업그레이드 (플레이어)"))).Font(HeaderFont).ColorAndOpacity(FLinearColor(0.55f, 0.8f, 1.f))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("전부 0단계")))
			.OnClicked_Lambda([this]()
			{
				if (Loadout.IsValid())
				{
					Loadout->ResetUpgrades();
					UpgradeMessage = FText::FromString(TEXT("업그레이드 초기화"));
				}
				return FReply::Handled();
			})
		]
	];
	Page->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.08f, 0.1f, 0.16f, 1.f))
		.Padding(8.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("로그라이크 업그레이드 시험: 단계를 올리면 그 스킬(액티브 / 패시브 따로)에 바로 반영된다. 정의는 Content/GameData/upgrades.json (고치면 바로 다시 읽음, 이 탭은 옵션창을 다시 열면 갱신). 콘솔: CRUpgrade <id> [단계], CRUpgradeReset.")))
			.Font(Font).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.88f, 0.95f))
		]
	];
	Page->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock).Text_Lambda([this]() { return UpgradeMessage; }).Font(Font).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.4f))
	];

	const UCRGameDataSubsystem* Data = Loadout.IsValid() ? UCRGameDataSubsystem::Get(Loadout.Get()) : nullptr;
	if (!Data)
	{
		Page->AddSlot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(TEXT("캐릭터로 플레이할 때만 (타격 체크 / 인게임)"))).Font(Font) ];
		return Page;
	}
	FString LastSkill;
	for (const FCRUpgradeDef& Def : Data->GetUpgrades())
	{
		const FString Skill = CRSkill::MakeSkillItem(Def.Family, Def.Form).ToString();
		if (Skill != LastSkill)
		{
			LastSkill = Skill;
			const FString Title = FString::Printf(TEXT("%s (%s)"), *CRSkill::GetFamilyLabel(Def.Family),
				Def.Form == ECRSkillForm::Active ? TEXT("액티브") : TEXT("패시브"));
			Page->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)
			[
				SNew(STextBlock).Text(FText::FromString(Title)).Font(HeaderFont).ColorAndOpacity(Def.Form == ECRSkillForm::Active
					? FLinearColor(0.5f, 0.75f, 1.f) : FLinearColor(0.5f, 1.f, 0.6f))
			];
		}
		const FName Id = Def.Id;
		const int32 MaxLevel = Def.MaxLevel;
		auto Change = [this, Id](int32 Delta)
		{
			if (Loadout.IsValid())
			{
				FString Message;
				Loadout->SetUpgradeLevel(Id, Loadout->GetUpgradeLevel(Id) + Delta, &Message);
				UpgradeMessage = FText::FromString(Message);
			}
			return FReply::Handled();
		};
		Page->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  ·  %s"), *Def.Name, *Def.Desc)))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("id: %s%s"), *Def.Id.ToString(),
					Def.Requires.IsNone() ? TEXT("") : *FString::Printf(TEXT(" · 선행: %s"), *Def.Requires.ToString()))))
				.Font(Font).AutoWrapText(true)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6.f, 0.f, 2.f, 0.f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("－"))).OnClicked_Lambda([Change]() { return Change(-1); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).WidthOverride(46.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this, Id, MaxLevel]()
					{
						const int32 Level = Loadout.IsValid() ? Loadout->GetUpgradeLevel(Id) : 0;
						return FText::FromString(FString::Printf(TEXT("%d / %d"), Level, MaxLevel));
					})
					.ColorAndOpacity_Lambda([this, Id]()
					{
						return Loadout.IsValid() && Loadout->GetUpgradeLevel(Id) > 0 ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.6f, 0.6f, 0.65f);
					})
					.Font(Font)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton).Text(FText::FromString(TEXT("＋"))).OnClicked_Lambda([Change]() { return Change(1); })
			]
		];
	}
	return Page;
}

TSharedRef<SWidget> SCRTuningPanel::MakeBenchmarkControls(const FArguments& InArgs, const FSlateFontInfo& Font)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ButtonColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.35f))
				.Text(FText::FromString(TEXT("▶ 벤치마크 시작")))
				.OnClicked_Lambda([this]()
				{
					OnStartBenchmark.ExecuteIfBound();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("중지 · 원래 맵으로")))
				.OnClicked_Lambda([this]()
				{
					OnStopBenchmark.ExecuteIfBound();
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(STextBlock).Text(InArgs._BenchmarkText).Font(Font).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.75f, 1.f, 0.8f))
		];
}

void SCRTuningPanel::ResetCategory(const FString& Category)
{
	if (!Subsystem.IsValid())
	{
		return;
	}
	// 기본값 = 코드 기본값 + GameData JSON
	const UCRTuningSettings* Defaults = Subsystem->GetDefaults() ? Subsystem->GetDefaults() : GetDefault<UCRTuningSettings>();
	for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
	{
		if (Item.Category.ToString() == Category)
		{
			Subsystem->SetValue(Item.GetName(), Item.GetValue(Defaults));
		}
	}
}

void SCRTuningPanel::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	OnCloseRequested = InArgs._OnCloseRequested;
	OnBackToTitle = InArgs._OnBackToTitle;
	OnPreview = InArgs._OnPreview;
	OnSpawnTargets = InArgs._OnSpawnTargets;
	OnClearTargets = InArgs._OnClearTargets;
	OnStartBattle = InArgs._OnStartBattle;
	OnStartBenchmark = InArgs._OnStartBenchmark;
	OnStopBenchmark = InArgs._OnStopBenchmark;
	OnStartWave = InArgs._OnStartWave;
	OnStopWave = InArgs._OnStopWave;
	Loadout = InArgs._Loadout;

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo RowFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);

	// 탭 = 레지스트리 카테고리 (효과 하나당 하나). 첫 탭은 몬스터/팀전 버튼.
	TArray<FString> TabNames;
	TArray<TSharedRef<SWidget>> Pages;
	if (InArgs._ShowSpawnPage)
	{
		TabNames.Add(TEXT("몬스터 · 팀전"));
		Pages.Add(MakeSpawnPage(InArgs, HeaderFont, RowFont));
	}
	if (Loadout.IsValid())
	{
		TabNames.Add(TEXT("업그레이드"));
		Pages.Add(MakeUpgradePage(HeaderFont, RowFont));
	}

	TSharedPtr<SVerticalBox> CurrentRows;
	for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
	{
		const FString Category = Item.Category.ToString();
		if (TabNames.IsEmpty() || TabNames.Last() != Category)
		{
			CurrentRows = SNew(SVerticalBox);
			CurrentRows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Item.Category).Font(HeaderFont).ColorAndOpacity(FLinearColor(0.55f, 0.8f, 1.f))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SButton)
					.Visibility(UCRTuningSettings::HasPreview(Category) ? EVisibility::Visible : EVisibility::Collapsed)
					.ButtonColorAndOpacity(FLinearColor(1.f, 0.55f, 0.15f))
					.ToolTipText(FText::FromString(TEXT("지금 수치로 플레이어 앞에 이 효과를 터뜨린다 (옵션창을 연 채로 수치를 바꾸며 비교)")))
					.Text(FText::FromString(TEXT("▶ 미리보기")))
					.OnClicked_Lambda([this, Category]()
					{
						OnPreview.ExecuteIfBound(Category);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("이 탭만 기본값")))
					.OnClicked_Lambda([this, Category]()
					{
						ResetCategory(Category);
						return FReply::Handled();
					})
				]
			];
			const FText Description = UCRTuningSettings::GetCategoryDescription(Category);
			if (!Description.IsEmpty())
			{
				CurrentRows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.08f, 0.1f, 0.16f, 1.f))
					.Padding(8.f)
					[
						SNew(STextBlock).Text(Description).Font(RowFont).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.88f, 0.95f))
					]
				];
			}
			if (Category == TEXT("벤치마크"))
			{
				CurrentRows->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[ MakeBenchmarkControls(InArgs, RowFont) ];
			}
			TabNames.Add(Category);
			Pages.Add(CurrentRows.ToSharedRef());
		}
		CurrentRows->AddSlot().AutoHeight().Padding(0.f, 2.f)[ MakeItemRow(Item, RowFont) ];
	}

	GLastTuningTab = FMath::Clamp(GLastTuningTab, 0, Pages.Num() - 1);

	TSharedRef<SVerticalBox> TabList = SNew(SVerticalBox);
	TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher).WidgetIndex_Lambda([]() { return GLastTuningTab; });
	for (int32 i = 0; i < Pages.Num(); ++i)
	{
		TabList->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity_Lambda([i]() { return i == GLastTuningTab ? TabSelectedColor : TabNormalColor; })
			.OnClicked_Lambda([i]()
			{
				GLastTuningTab = i;
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FText::FromString(TabNames[i])).Font(RowFont)
			]
		];
		Switcher->AddSlot()[ SNew(SScrollBox) + SScrollBox::Slot()[ Pages[i] ] ];
	}

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(24.f)
	[
		SNew(SBox)
		.WidthOverride(720.f)
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
					.Text(FText::FromString(TEXT("Tab: 닫기 · 값은 자동 저장 · 왼쪽에서 효과를 고르면 그 효과의 수치만 보임")))
					.Font(RowFont)
					.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f))
				]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
					[
						SNew(SBox).WidthOverride(170.f)
						[
							SNew(SScrollBox) + SScrollBox::Slot()[ TabList ]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						Switcher
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("전체 기본값으로 초기화")))
						.OnClicked_Lambda([this]()
						{
							if (Subsystem.IsValid())
							{
								Subsystem->ResetToDefaults();
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(FText::FromString(TEXT("타이틀로")))
						.OnClicked_Lambda([this]()
						{
							OnBackToTitle.ExecuteIfBound();
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

FReply SCRTuningPanel::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		OnCloseRequested.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
