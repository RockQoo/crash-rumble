#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CRGameMode.generated.h"

UCLASS()
class CRASHRUMBLE_API ACRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACRGameMode();

protected:
	virtual void BeginPlay() override;
};
