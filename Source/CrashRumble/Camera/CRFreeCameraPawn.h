#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "CRFreeCameraPawn.generated.h"

/**
 * 맵 제작 모드의 자유 카메라. 입력은 ACRPlayerController(Enhanced Input)가 넣고,
 * 속도는 튜닝(FreeCameraSpeed / FreeCameraFastMultiplier)을 매 틱 반영한다.
 */
UCLASS()
class CRASHRUMBLE_API ACRFreeCameraPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	ACRFreeCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	void SetFast(bool bInFast) { bFast = bInFast; }

private:
	bool bFast = false;
};
