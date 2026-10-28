#include "Core/SBHUD.h"

#include "Camera/SBViewComponent.h"
#include "Character/SBSnowmanCharacter.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void ASBHUD::DrawHUD()
{
	Super::DrawHUD();

	const ASBSnowmanCharacter* Snowman = PlayerOwner ? PlayerOwner->GetPawn<ASBSnowmanCharacter>() : nullptr;
	if (!Snowman || !Canvas)
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
