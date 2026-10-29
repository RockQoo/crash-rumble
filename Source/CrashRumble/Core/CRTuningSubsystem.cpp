#include "Core/CRTuningSubsystem.h"

#include "Core/CRTuningSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* SaveSlotName = TEXT("CRTuning");
}

UCRTuningSubsystem* UCRTuningSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCRTuningSubsystem>() : nullptr;
}

const UCRTuningSettings& UCRTuningSubsystem::GetSettings(const UObject* WorldContext)
{
	const UCRTuningSubsystem* Subsystem = Get(WorldContext);
	return Subsystem && Subsystem->Settings ? *Subsystem->Settings : *GetDefault<UCRTuningSettings>();
}

void UCRTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Settings = NewObject<UCRTuningSettings>(this);
	Load();

	TuneCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("cr.Tune"),
		TEXT("튜닝 값 조회/변경. cr.Tune (목록) | cr.Tune <이름> <값> | cr.Tune reset"),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UCRTuningSubsystem::HandleTuneCommand),
		ECVF_Default);
}

void UCRTuningSubsystem::Deinitialize()
{
	if (TuneCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(TuneCommand);
		TuneCommand = nullptr;
	}
	Save();
	Super::Deinitialize();
}

void UCRTuningSubsystem::SetValue(FName Name, float Value)
{
	if (const FCRTuningItem* Item = UCRTuningSettings::FindItem(Name))
	{
		Item->SetValue(Settings, Value);
		NotifyChanged();
	}
}

void UCRTuningSubsystem::ResetToDefaults()
{
	const UCRTuningSettings* Defaults = GetDefault<UCRTuningSettings>();
	for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
	{
		Item.SetValue(Settings, Item.GetValue(Defaults));
	}
	NotifyChanged();
	Save();
}

void UCRTuningSubsystem::Save()
{
	// 기본값과 다른 값만 저장 → 코드 기본값을 바꾸면 손대지 않은 항목은 새 기본값을 따른다
	UCRTuningSave* SaveObject = NewObject<UCRTuningSave>(this);
	SaveObject->Version = UCRTuningSave::CurrentVersion;
	const UCRTuningSettings* Defaults = GetDefault<UCRTuningSettings>();
	for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
	{
		const float Value = Item.GetValue(Settings);
		if (!FMath::IsNearlyEqual(Value, Item.GetValue(Defaults), 1e-4f))
		{
			SaveObject->Values.Add(Item.GetName(), Value);
		}
	}
	UGameplayStatics::SaveGameToSlot(SaveObject, SaveSlotName, 0);
}

void UCRTuningSubsystem::Load()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return;
	}
	const UCRTuningSave* SaveObject = Cast<UCRTuningSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	// 버전 0은 모든 값을 통째로 저장해 기본값 변경을 가리므로 버린다
	if (SaveObject && SaveObject->Version >= UCRTuningSave::CurrentVersion)
	{
		for (const TPair<FName, float>& Pair : SaveObject->Values)
		{
			if (const FCRTuningItem* Item = UCRTuningSettings::FindItem(Pair.Key))
			{
				Item->SetValue(Settings, Pair.Value);
			}
		}
	}
}

void UCRTuningSubsystem::HandleTuneCommand(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		for (const FCRTuningItem& Item : UCRTuningSettings::GetItems())
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
