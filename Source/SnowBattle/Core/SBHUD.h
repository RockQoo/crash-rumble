#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SBHUD.generated.h"

/**
 * 숄더뷰 십자선, 데미지 숫자, 머리 위 체력바.
 * 데미지 숫자는 USBFeedbackSubsystem의 고정 버퍼를 읽어 캔버스에 그린다 (위젯 생성 없음).
 */
UCLASS()
class SNOWBATTLE_API ASBHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair();
	void DrawHealthBars();
	void DrawDamageNumbers();
};
