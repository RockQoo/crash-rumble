#include "Core/CRMapEditGameMode.h"

#include "Camera/CRFreeCameraPawn.h"
#include "Core/CRHUD.h"
#include "Core/CRPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Map/CRBlockArena.h"

ACRMapEditGameMode::ACRMapEditGameMode()
{
	DefaultPawnClass = ACRFreeCameraPawn::StaticClass();
	PlayerControllerClass = ACRPlayerController::StaticClass();
	HUDClass = ACRHUD::StaticClass();
}

void ACRMapEditGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MapFile = UGameplayStatics::ParseOption(Options, TEXT("map"));
	if (MapFile.IsEmpty())
	{
		MapFile = TEXT("arena");
	}
}

void ACRMapEditGameMode::BeginPlay()
{
	Super::BeginPlay();
	FString Message;
	ACRBlockArena::LoadMap(GetWorld(), MapFile, Message);
}
