#include "Core/CRModes.h"

#include "Core/CRGameSettings.h"
#include "Core/CRInGameGameMode.h"
#include "Core/CRMapEditGameMode.h"
#include "Core/CRTitleGameMode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr ECRMode AllModes[] = { ECRMode::Title, ECRMode::HitCheck, ECRMode::MapEdit, ECRMode::InGame };

	FAutoConsoleCommandWithWorldAndArgs ModeCommand(
		TEXT("CRMode"),
		TEXT("모드 전환: CRMode Title | HitCheck | MapEdit | InGame"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0 || !World)
			{
				return;
			}
			for (const ECRMode Mode : AllModes)
			{
				if (Args[0].Equals(CRModes::GetAlias(Mode), ESearchCase::IgnoreCase))
				{
					CRModes::Open(World, Mode);
					return;
				}
			}
		}));
}

void CRModes::Open(const UObject* WorldContext, ECRMode Mode)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContext, GetDefault<UCRGameSettings>()->ModeMap, true,
		FString::Printf(TEXT("game=%s"), GetAlias(Mode)));
}

ECRMode CRModes::GetCurrent(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (Cast<ACRTitleGameMode>(GameMode))
	{
		return ECRMode::Title;
	}
	if (Cast<ACRMapEditGameMode>(GameMode))
	{
		return ECRMode::MapEdit;
	}
	// 인게임은 타격 체크(ACRGameMode)를 상속하므로 먼저 검사
	if (Cast<ACRInGameGameMode>(GameMode))
	{
		return ECRMode::InGame;
	}
	return ECRMode::HitCheck;
}

FText CRModes::GetDisplayName(ECRMode Mode)
{
	switch (Mode)
	{
	case ECRMode::Title:   return FText::FromString(TEXT("타이틀"));
	case ECRMode::MapEdit: return FText::FromString(TEXT("맵 제작"));
	case ECRMode::InGame:  return FText::FromString(TEXT("인게임"));
	default:               return FText::FromString(TEXT("타격 체크"));
	}
}

const TCHAR* CRModes::GetAlias(ECRMode Mode)
{
	switch (Mode)
	{
	case ECRMode::Title:   return TEXT("Title");
	case ECRMode::MapEdit: return TEXT("MapEdit");
	case ECRMode::InGame:  return TEXT("InGame");
	default:               return TEXT("HitCheck");
	}
}
