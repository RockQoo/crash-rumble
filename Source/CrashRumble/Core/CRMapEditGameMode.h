#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRMapEditGameMode.generated.h"

/**
 * 맵 제작 모드: 맵 파일(JSON)을 불러와 자유 카메라로 둘러보고, 블럭을 부숴 붕괴/재생을 확인한다.
 * 파일을 고치면 아레나가 알아서 다시 불러온다 (핫 리로드).
 */
UCLASS()
class CRASHRUMBLE_API ACRMapEditGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRMapEditGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	virtual void BeginPlay() override;

private:
	/** URL 옵션 ?map=<이름>, 없으면 arena. Content/MapData/<이름>.json */
	FString MapFile;
};
