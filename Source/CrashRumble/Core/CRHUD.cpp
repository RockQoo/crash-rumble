#include "Core/CRHUD.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "Character/CRLoadoutComponent.h"
#include "Core/CRWaveSubsystem.h"
#include "Weapon/CRSkill.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float HealthBarWidth = 80.f;
	constexpr float HealthBarHeight = 7.f;
	constexpr float HealthBarHeightAboveActor = 130.f;
}

void ACRHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}
	DrawHealthBars();
	DrawDamageNumbers();
	DrawCrosshair();
	DrawLoadoutBar();
	DrawWaveInfo();
}

void ACRHUD::DrawCrosshair()
{
	const ACRCharacter* Snowman = PlayerOwner ? PlayerOwner->GetPawn<ACRCharacter>() : nullptr;
	if (!Snowman)
	{
		return;
	}

	// 탑다운으로 넘어갈수록 흐려짐
	const float Alpha = 1.f - Snowman->GetViewComponent()->GetBlendAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}

	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const FLinearColor Color(1.f, 1.f, 1.f, 0.85f * Alpha);
	constexpr float Gap = 6.f;
	constexpr float Length = 10.f;
	constexpr float Thickness = 2.f;

	DrawLine(Center.X - Gap - Length, Center.Y, Center.X - Gap, Center.Y, Color, Thickness);
	DrawLine(Center.X + Gap, Center.Y, Center.X + Gap + Length, Center.Y, Color, Thickness);
	DrawLine(Center.X, Center.Y - Gap - Length, Center.X, Center.Y - Gap, Color, Thickness);
	DrawLine(Center.X, Center.Y + Gap, Center.X, Center.Y + Gap + Length, Color, Thickness);
	DrawRect(Color, Center.X - 1.f, Center.Y - 1.f, 2.f, 2.f);
}

void ACRHUD::DrawHealthBars()
{
	const APawn* LocalPawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;

	for (TActorIterator<ACRCharacter> It(GetWorld()); It; ++It)
	{
		const ACRCharacter* Snowman = *It;
		if (Snowman->GetTrainingRole() == ECRTrainingRole::DpsDummy && !Snowman->IsHidden())
		{
			// DPS 허수아비: 머리 위에 초당 / 누적 데미지
			const FVector Screen = Project(Snowman->GetActorLocation() + FVector(0.f, 0.f, HealthBarHeightAboveActor), true);
			if (Screen.Z > 0.f && GEngine)
			{
				const FString Text = FString::Printf(TEXT("DPS %.0f  ·  누적 %.0f"), Snowman->GetDps(), Snowman->GetTotalDamage());
				float Width = 0.f, Height = 0.f;
				UFont* Font = GEngine->GetMediumFont();
				GetTextSize(Text, Width, Height, Font, 1.2f);
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), Screen.X - Width * 0.5f - 6.f, Screen.Y - 4.f, Width + 12.f, Height + 8.f);
				DrawText(Text, FLinearColor(1.f, 0.85f, 0.2f), Screen.X - Width * 0.5f, Screen.Y, Font, 1.2f);
			}
			continue;
		}
		// 수백 마리일 때를 위해 맞아서 체력이 줄어든 대상만 표시
		if (Snowman == LocalPawn || Snowman->IsHidden() || Snowman->IsDead() || Snowman->GetMaxHealth() <= 0.f
			|| Snowman->GetHealth() >= Snowman->GetMaxHealth())
		{
			continue;
		}

		const FVector WorldPos = Snowman->GetActorLocation() + FVector(0.f, 0.f, HealthBarHeightAboveActor);
		const FVector Screen = Project(WorldPos, true);
		if (Screen.Z <= 0.f)
		{
			continue; // 카메라 뒤
		}

		const float Ratio = FMath::Clamp(Snowman->GetHealth() / Snowman->GetMaxHealth(), 0.f, 1.f);
		const float X = Screen.X - HealthBarWidth * 0.5f;
		const float Y = Screen.Y;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 1.f, Y - 1.f, HealthBarWidth + 2.f, HealthBarHeight + 2.f);
		const FLinearColor Fill = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.15f, 0.1f), FLinearColor(0.3f, 1.f, 0.3f), Ratio);
		DrawRect(Fill, X, Y, HealthBarWidth * Ratio, HealthBarHeight);
	}
}

void ACRHUD::DrawDamageNumbers()
{
	const UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(this);
	if (!Feedback || !GEngine)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UFont* Font = GEngine->GetLargeFont();

	for (const FCRDamageNumber& Number : Feedback->GetDamageNumbers())
	{
		if (Number.Age >= Number.Duration)
		{
			continue;
		}
		const float T = Number.Age / FMath::Max(Number.Duration, 0.01f);

		// 위로 떠오르며 감속, 처음엔 크게 튀었다가 줄어듦, 마지막 40%에서 사라짐
		const float Rise = Tuning.DamageNumberRise * (1.f - FMath::Square(1.f - T));
		const FVector Screen = Project(Number.WorldLocation + FVector(0.f, 0.f, 40.f + Rise), true);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float Pop = T < 0.12f ? FMath::Lerp(1.6f, 1.f, T / 0.12f) : 1.f;
		const float Alpha = T > 0.6f ? 1.f - (T - 0.6f) / 0.4f : 1.f;
		const float Scale = Tuning.DamageNumberScale * Pop * Number.Scale;

		const FString Text = FString::Printf(TEXT("%d"), FMath::RoundToInt(Number.Value));
		float Width = 0.f, Height = 0.f;
		Canvas->TextSize(Font, Text, Width, Height, Scale, Scale);

		FCanvasTextItem Item(
			FVector2D(Screen.X + Number.ScreenJitter.X - Width * 0.5f, Screen.Y + Number.ScreenJitter.Y - Height * 0.5f),
			FText::FromString(Text), Font, FLinearColor(Number.Color.R, Number.Color.G, Number.Color.B, Alpha));
		Item.Scale = FVector2D(Scale, Scale);
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, Alpha);
		Canvas->DrawItem(Item);
	}
}

void ACRHUD::DrawSlot(float X, float Y, float Size, const FString& Label, const FString& Key, const FLinearColor& Color, float Cooldown)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, Size + 4.f, Size + 4.f);
	DrawRect(Label.IsEmpty() ? FLinearColor(0.12f, 0.13f, 0.16f, 0.8f) : Color, X, Y, Size, Size);
	if (Cooldown > 0.f)
	{
		// 쿨다운: 위에서부터 어둡게 덮였다가 줄어든다
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X, Y, Size, Size * Cooldown);
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}
	if (!Label.IsEmpty())
	{
		float Width = 0.f, Height = 0.f;
		GetTextSize(Label, Width, Height, Font, 1.f);
		DrawText(Label, FLinearColor::White, X + (Size - Width) * 0.5f, Y + (Size - Height) * 0.5f, Font, 1.f);
	}
	if (!Key.IsEmpty())
	{
		DrawText(Key, FLinearColor(1.f, 1.f, 1.f, 0.7f), X + 3.f, Y + 1.f, GEngine->GetSmallFont(), 1.f);
	}
}

void ACRHUD::DrawLoadoutBar()
{
	const ACRCharacter* Snowman = PlayerOwner ? PlayerOwner->GetPawn<ACRCharacter>() : nullptr;
	const UCRLoadoutComponent* Loadout = Snowman ? Snowman->GetLoadout() : nullptr;
	if (!Loadout || !GEngine || Snowman->IsHidden())
	{
		return;
	}
	const float Size = 54.f;
	const float Gap = 6.f;
	const float BarWidth = 2.f * Size + Gap + 24.f + 4.f * Size + 3.f * Gap;
	float X = (Canvas->ClipX - BarWidth) * 0.5f;
	const float Y = Canvas->ClipY - Size - 70.f;

	// 내 체력 바
	const float HealthRatio = Snowman->GetMaxHealth() > 0.f ? FMath::Clamp(Snowman->GetHealth() / Snowman->GetMaxHealth(), 0.f, 1.f) : 0.f;
	const float HealthY = Y - 46.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, HealthY - 2.f, BarWidth + 4.f, 14.f);
	DrawRect(FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.15f, 0.1f), FLinearColor(0.3f, 1.f, 0.3f), HealthRatio), X, HealthY, BarWidth * HealthRatio, 10.f);
	DrawText(FString::Printf(TEXT("%.0f / %.0f"), Snowman->GetHealth(), Snowman->GetMaxHealth()), FLinearColor::White, X + BarWidth + 10.f, HealthY - 4.f, GEngine->GetSmallFont(), 1.f);

	DrawText(FString::Printf(TEXT("프리셋 %d  (X)"), Loadout->GetPresetIndex() + 1), FLinearColor(0.7f, 0.85f, 1.f), X, Y - 22.f, GEngine->GetSmallFont(), 1.f);
	for (int32 i = 0; i < FCRLoadoutPreset::ActiveCount; ++i)
	{
		const ECRWeaponType Family = Loadout->GetActiveFamily(i);
		const FString Label = Family != ECRWeaponType::None ? CRSkill::GetFamilyLabel(Family) : (i == 1 ? TEXT("펀치") : TEXT(""));
		DrawSlot(X, Y, Size, Label, i == 0 ? TEXT("좌") : TEXT("우"), FLinearColor(0.16f, 0.3f, 0.6f, 0.85f), 0.f);
		X += Size + Gap;
	}
	X += 24.f - Gap;
	for (int32 i = 0; i < FCRLoadoutPreset::PassiveCount; ++i)
	{
		const ECRWeaponType Family = Loadout->GetPassiveFamily(i);
		const FString Label = Family != ECRWeaponType::None ? CRSkill::GetFamilyLabel(Family) : TEXT("");
		DrawSlot(X, Y, Size, Label, TEXT("자동"), FLinearColor(0.14f, 0.45f, 0.25f, 0.85f),
			Family != ECRWeaponType::None && Family != ECRWeaponType::Sword ? Loadout->GetPassiveCooldownRatio(i) : 0.f);
		X += Size + Gap;
	}

	// 도플갱어 상태
	FString Doppel = TEXT("G 도플갱어: 준비");
	if (Snowman->GetDoppel())
	{
		Doppel = TEXT("G 도플갱어: 함께 싸우는 중");
	}
	else if (const float Left = Snowman->GetDoppelCooldownLeft(); Left > 0.f)
	{
		Doppel = FString::Printf(TEXT("G 도플갱어: %.1f초"), Left);
	}
	DrawText(Doppel, FLinearColor(0.75f, 0.55f, 1.f), (Canvas->ClipX - BarWidth) * 0.5f, Y + Size + 8.f, GEngine->GetSmallFont(), 1.f);

	// 오른쪽 위: 골드 (드르륵 올라가는 숫자)
	UFont* Large = GEngine->GetLargeFont();
	const FString Gold = FString::Printf(TEXT("골드 %d"), Loadout->GetDisplayedGold());
	float Width = 0.f, Height = 0.f;
	GetTextSize(Gold, Width, Height, Large, 1.4f);
	const bool bRolling = Loadout->GetDisplayedGold() != Loadout->GetGold();
	FCanvasTextItem Item(FVector2D(Canvas->ClipX - Width - 24.f, 20.f), FText::FromString(Gold), Large,
		bRolling ? FLinearColor(1.f, 1.f, 0.6f) : FLinearColor(1.f, 0.82f, 0.2f));
	Item.Scale = FVector2D(bRolling ? 1.5f : 1.4f);
	Item.bOutlined = true;
	Item.OutlineColor = FLinearColor::Black;
	Canvas->DrawItem(Item);
}

void ACRHUD::DrawWaveInfo()
{
	const UCRWaveSubsystem* Wave = UCRWaveSubsystem::Get(this);
	if (!Wave || !Wave->IsRunning() || !GEngine)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const int32 Seconds = FMath::FloorToInt(Wave->GetElapsed());
	const FString Info = FString::Printf(TEXT("생존 %d:%02d  ·  처치 %d  ·  몬스터 %d"), Seconds / 60, Seconds % 60, Wave->GetKills(), Wave->GetAliveCount());
	float Width = 0.f, Height = 0.f;
	GetTextSize(Info, Width, Height, Font, 1.2f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), (Canvas->ClipX - Width) * 0.5f - 10.f, 14.f, Width + 20.f, Height + 10.f);
	DrawText(Info, FLinearColor(1.f, 0.9f, 0.8f), (Canvas->ClipX - Width) * 0.5f, 19.f, Font, 1.2f);

	if (!Wave->IsGameOver())
	{
		return;
	}
	// 게임 오버 결과
	const float BoxWidth = 520.f;
	const float BoxHeight = 250.f;
	const float BoxX = (Canvas->ClipX - BoxWidth) * 0.5f;
	const float BoxY = (Canvas->ClipY - BoxHeight) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), BoxX, BoxY, BoxWidth, BoxHeight);
	UFont* Large = GEngine->GetLargeFont();
	auto Centered = [this, BoxX, BoxWidth](const FString& Text, float Y, UFont* InFont, float Scale, const FLinearColor& Color)
	{
		float TextWidth = 0.f, TextHeight = 0.f;
		GetTextSize(Text, TextWidth, TextHeight, InFont, Scale);
		DrawText(Text, Color, BoxX + (BoxWidth - TextWidth) * 0.5f, Y, InFont, Scale);
	};
	Centered(TEXT("게임 오버"), BoxY + 24.f, Large, 2.f, FLinearColor(1.f, 0.3f, 0.25f));
	Centered(FString::Printf(TEXT("생존 시간 %d:%02d"), Seconds / 60, Seconds % 60), BoxY + 90.f, Font, 1.3f, FLinearColor::White);
	Centered(FString::Printf(TEXT("처치 %d"), Wave->GetKills()), BoxY + 120.f, Font, 1.3f, FLinearColor::White);
	Centered(FString::Printf(TEXT("골드 %d"), Wave->GetFinalGold()), BoxY + 150.f, Font, 1.3f, FLinearColor(1.f, 0.82f, 0.2f));
	Centered(TEXT("R 다시 시작  ·  Esc 타이틀"), BoxY + 200.f, Font, 1.1f, FLinearColor(0.75f, 0.85f, 1.f));
}
