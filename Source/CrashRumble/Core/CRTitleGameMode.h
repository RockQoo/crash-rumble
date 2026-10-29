#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRTitleGameMode.generated.h"

/** 타이틀 화면. 폰 없이 맵을 배경으로 카메라만 돈다. */
UCLASS()
class CRASHRUMBLE_API ACRTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRTitleGameMode();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
