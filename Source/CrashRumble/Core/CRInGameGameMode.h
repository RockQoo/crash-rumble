#pragma once

#include "CoreMinimal.h"
#include "Core/CRGameMode.h"
#include "CRInGameGameMode.generated.h"

/**
 * 인게임 테스트 판: 평평한 블럭 그리드(지형 복구 없음), 탑뷰 고정, 북쪽에서 계속 오는 몬스터 웨이브,
 * 골드 줍기, 인벤토리 장착, 도플갱어. 캐릭터/전투/타격감은 타격 체크와 같다.
 * 맵은 URL 옵션 `?map=<이름>` (없으면 ingame)으로 블럭 아레나를 만든다.
 */
UCLASS()
class CRASHRUMBLE_API ACRInGameGameMode : public ACRGameMode
{
	GENERATED_BODY()

public:
	ACRInGameGameMode();

protected:
	virtual void BeginPlay() override;
};
