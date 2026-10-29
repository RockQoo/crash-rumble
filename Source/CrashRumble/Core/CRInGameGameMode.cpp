#include "Core/CRInGameGameMode.h"

#include "Kismet/GameplayStatics.h"

void ACRInGameGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MapFile = UGameplayStatics::ParseOption(Options, TEXT("map"));
}
