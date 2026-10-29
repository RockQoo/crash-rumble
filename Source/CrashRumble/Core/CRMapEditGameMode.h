#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRMapEditGameMode.generated.h"

/**
 * 맵 제작 모드: 자유 카메라로 맵을 둘러본다.
 * 맵 파일(JSON) 불러오기 / 블럭 부수기 / 핫 리로드는 11단계에서 여기에 붙인다.
 */
UCLASS()
class CRASHRUMBLE_API ACRMapEditGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRMapEditGameMode();
};
