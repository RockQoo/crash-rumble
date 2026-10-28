#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SBHUD.generated.h"

/** 숄더뷰 십자선. 탑다운에서는 마우스 커서가 조준점이라 그리지 않는다. */
UCLASS()
class SNOWBATTLE_API ASBHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
