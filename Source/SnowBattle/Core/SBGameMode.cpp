#include "Core/SBGameMode.h"

#include "Character/SBSnowmanCharacter.h"
#include "Core/SBHUD.h"
#include "Core/SBPlayerController.h"

ASBGameMode::ASBGameMode()
{
	DefaultPawnClass = ASBSnowmanCharacter::StaticClass();
	PlayerControllerClass = ASBPlayerController::StaticClass();
	HUDClass = ASBHUD::StaticClass();
}
