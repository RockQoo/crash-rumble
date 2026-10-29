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
		const float Scale = Tuning.DamageNumberScale * Pop;

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
