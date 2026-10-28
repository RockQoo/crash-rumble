#include "Core/SBTuningSubsystem.h"

#include "Core/SBTuningSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* SaveSlotName = TEXT("SBTuning");
}

USBTuningSubsystem* USBTuningSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USBTuningSubsystem>() : nullptr;
}

const USBTuningSettings& USBTuningSubsystem::GetSettings(const UObject* WorldContext)
{
	const USBTuningSubsystem* Subsystem = Get(WorldContext);
	return Subsystem && Subsystem->Settings ? *Subsystem->Settings : *GetDefault<USBTuningSettings>();
}

void USBTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Settings = NewObject<USBTuningSettings>(this);
	Load();

	TuneCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("sb.Tune"),
		TEXT("튜닝 값 조회/변경. sb.Tune (목록) | sb.Tune <이름> <값> | sb.Tune reset"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &USBTuningSubsystem::HandleTuneCommand),
		ECVF_Default);
}

void USBTuningSubsystem::Deinitialize()
{
	if (TuneCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(TuneCommand);
		TuneCommand = nullptr;
	}
	Save();
	Super::Deinitialize();
}

void USBTuningSubsystem::SetValue(FName Name, float Value)
{
	if (const FSBTuningItem* Item = USBTuningSettings::FindItem(Name))
	{
		Item->SetValue(Settings, Value);
		NotifyChanged();
	}
}

void USBTuningSubsystem::ResetToDefaults()
{
	const USBTuningSettings* Defaults = GetDefault<USBTuningSettings>();
	for (const FSBTuningItem& Item : USBTuningSettings::GetItems())
	{
		Item.SetValue(Settings, Item.GetValue(Defaults));
	}
	NotifyChanged();
	Save();
}

void USBTuningSubsystem::Save()
{
	USBTuningSave* SaveObject = NewObject<USBTuningSave>(this);
	for (const FSBTuningItem& Item : USBTuningSettings::GetItems())
	{
		SaveObject->Values.Add(Item.GetName(), Item.GetValue(Settings));
	}
	UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, 0);
}

void USBTuningSubsystem::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return;
	}
	if (const USBTuningSave* SaveObject = Cast<USBTuningSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
	{
		for (const TPair<FName, float>& Pair : SaveObject->Values)
		{
			if (const FSBTuningItem* Item = USBTuningSettings::FindItem(Pair.Key))
			{
				Item->SetValue(Settings, Pair.Value);
			}
		}
	}
}

void USBTuningSubsystem::HandleTuneCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		for (const FSBTuningItem& Item : USBTuningSettings::GetItems())
		{
			UE_LOG(LogTemp, Display, TEXT("%s = %g  (%s, %g ~ %g)"),
				*Item.GetName().ToString(), Item.GetValue(Settings), *Item.Label.ToString(), Item.Min, Item.Max);
		}
		return;
	}
	if (Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		ResetToDefaults();
		return;
	}
	if (Args.Num() >= 2)
	{
		SetValue(FName(*Args[0]), FCString::Atof(*Args[1]));
		Save();
	}
}
