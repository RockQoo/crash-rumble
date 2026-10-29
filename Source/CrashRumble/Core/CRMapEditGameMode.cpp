#include "Core/CRMapEditGameMode.h"

#include "Camera/CRFreeCameraPawn.h"
#include "Core/CRHUD.h"
#include "Core/CRPlayerController.h"

ACRMapEditGameMode::ACRMapEditGameMode()
{
	DefaultPawnClass = ACRFreeCameraPawn::StaticClass();
	PlayerControllerClass = ACRPlayerController::StaticClass();
	HUDClass = ACRHUD::StaticClass();
}
