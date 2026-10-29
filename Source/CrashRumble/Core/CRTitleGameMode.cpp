#include "Core/CRTitleGameMode.h"

#include "Core/CRTitlePlayerController.h"
#include "GameFramework/HUD.h"

ACRTitleGameMode::ACRTitleGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACRTitlePlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void ACRTitleGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// 폰을 스폰하지 않는다 (RestartPlayer 생략)
}
