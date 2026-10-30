#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CRHUD.generated.h"

/**
 * 숄더뷰 십자선, 데미지 숫자, 머리 위 체력바, 장착 스킬 바·골드, 인게임 웨이브 정보·결과.
 * 데미지 숫자는 UCRFeedbackSubsystem의 고정 버퍼를 읽어 캔버스에 그린다 (위젯 생성 없음).
 */
UCLASS()
class CRASHRUMBLE_API ACRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair();
	void DrawHealthBars();
	void DrawDamageNumbers();
	/** 아래 가운데: 프리셋, 액티브 2칸(좌/우클릭), 패시브 4칸(쿨다운), 도플갱어. 오른쪽 위: 골드. */
	void DrawLoadoutBar();
	/** 인게임 판: 생존 시간·처치·남은 몬스터, 게임 오버 결과. */
	void DrawWaveInfo();
	void DrawSlot(float X, float Y, float Size, const FString& Label, const FString& Key, const FLinearColor& Color, float Cooldown);
};
