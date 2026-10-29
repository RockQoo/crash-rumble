#pragma once

#include "CoreMinimal.h"
#include "Core/CRGameMode.h"
#include "CRInGameGameMode.generated.h"

/**
 * 인게임 모드 틀. 캐릭터/전투/타격감은 타격 체크와 같고, 몬스터 생성 도구는 숨긴다.
 * 내용(장르, 코어 루프)은 타격 테스트가 끝난 뒤 정한다.
 * 맵은 URL 옵션 `?map=<이름>` (없으면 arena)으로 블럭 아레나를 만든다.
 */
UCLASS()
class CRASHRUMBLE_API ACRInGameGameMode : public ACRGameMode
{
	GENERATED_BODY()

public:
	ACRInGameGameMode();
};
