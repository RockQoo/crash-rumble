#include "Core/CRInGameGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "Map/CRBlockArena.h"

void ACRInGameGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MapFile = UGameplayStatics::ParseOption(Options, TEXT("map"));
	if (MapFile.IsEmpty())
	{
		MapFile = TEXT("arena");
	}
}

void ACRInGameGameMode::BeginPlay()
{
	Super::BeginPlay();
	FString Message;
	ACRBlockArena::LoadMap(GetWorld(), MapFile, Message);
}
