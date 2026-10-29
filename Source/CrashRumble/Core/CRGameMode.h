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

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** 불러온 블럭 맵 파일 이름 (URL 옵션 ?map=, 없으면 DefaultMapFile, none이면 블럭 맵 없음). */
	const FString& GetMapFile() const { return MapFile; }

protected:
	virtual void BeginPlay() override;

	/** 타격 체크 = 훈련장 맵 (Content/MapData/training.json). */
	FString DefaultMapFile = TEXT("training");

private:
	FString MapFile;
};
