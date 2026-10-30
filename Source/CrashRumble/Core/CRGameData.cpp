#include "Core/CRGameData.h"

#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Map/CRMapFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr float PollInterval = 1.f;
	constexpr int32 MaxUpgradeLevel = 20;

	FLinearColor ReadColor(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FLinearColor& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Object->TryGetArrayField(Field, Array) || Array->Num() < 3)
		{
			return Fallback;
		}
		return FLinearColor((*Array)[0]->AsNumber(), (*Array)[1]->AsNumber(), (*Array)[2]->AsNumber(), 1.f);
	}
}

UCRGameDataSubsystem* UCRGameDataSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCRGameDataSubsystem>() : nullptr;
}

FString UCRGameDataSubsystem::GetDataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("GameData"));
}

void UCRGameDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UCRTuningSubsystem>();
	Super::Initialize(Collection);
	Reload();
#if !UE_BUILD_SHIPPING
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCRGameDataSubsystem::PollFiles), PollInterval);
#endif
}

void UCRGameDataSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
	Super::Deinitialize();
}

const FCRUpgradeDef* UCRGameDataSubsystem::FindUpgrade(FName Id) const
{
	return Upgrades.FindByPredicate([Id](const FCRUpgradeDef& Def) { return Def.Id == Id; });
}

const FCRHelmetDef* UCRGameDataSubsystem::FindHelmet(FName Id) const
{
	return Helmets.FindByPredicate([Id](const FCRHelmetDef& Def) { return Def.Id == Id; });
}

bool UCRGameDataSubsystem::PollFiles(float DeltaTime)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(GetDataDir(), TEXT("*.json")), true, false);
	bool bChanged = Files.Num() != FileStamps.Num();
	for (const FString& File : Files)
	{
		const FDateTime* Known = FileStamps.Find(File);
		if (!Known || *Known != IFileManager::Get().GetTimeStamp(*FPaths::Combine(GetDataDir(), File)))
		{
			bChanged = true;
			break;
		}
	}
	if (bChanged)
	{
		Reload();
		UE_LOG(LogTemp, Display, TEXT("GameData: 다시 읽음 (업그레이드 %d, 투구 %d, 경고 %d)"), Upgrades.Num(), Helmets.Num(), Warnings.Num());
	}
	return true;
}

void UCRGameDataSubsystem::Reload()
{
	Upgrades.Reset();
	Helmets.Reset();
	Warnings.Reset();
	FileStamps.Reset();
	CRMapFile::ResetBlockOverrides();

	TMap<FName, float> Tuning;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(GetDataDir(), TEXT("*.json")), true, false);
	Files.Sort();
	for (const FString& File : Files)
	{
		const FString Path = FPaths::Combine(GetDataDir(), File);
		FileStamps.Add(File, IFileManager::Get().GetTimeStamp(*Path));
		ParseFile(Path, Tuning);
	}
	for (const FString& Warning : Warnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameData: %s"), *Warning);
	}

	if (UCRTuningSubsystem* TuningSubsystem = GetGameInstance()->GetSubsystem<UCRTuningSubsystem>())
	{
		TuningSubsystem->ApplyJsonDefaults(Tuning);
	}
	OnChanged.Broadcast();
}

void UCRGameDataSubsystem::ParseFile(const FString& Path, TMap<FName, float>& OutTuning)
{
	const FString File = FPaths::GetCleanFilename(Path);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Warnings.Add(FString::Printf(TEXT("%s: JSON을 읽을 수 없음"), *File));
		return;
	}

	// 튜닝 기본값: { "tuning": { "Sword": { "Damage": 25 } } } → SwordDamage
	if (const TSharedPtr<FJsonObject>* TuningObject = nullptr; Root->TryGetObjectField(TEXT("tuning"), TuningObject))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Section : (*TuningObject)->Values)
		{
			const TSharedPtr<FJsonObject>* Fields = nullptr;
			if (!Section.Value->TryGetObject(Fields))
			{
				Warnings.Add(FString::Printf(TEXT("%s: tuning.%s 는 객체여야 함"), *File, *Section.Key));
				continue;
			}
			const FString Prefix = Section.Key.Equals(TEXT("Raw"), ESearchCase::IgnoreCase) ? FString() : Section.Key;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Fields)->Values)
			{
				const FName Name(*(Prefix + Field.Key));
				double Number = 0.0;
				bool bBool = false;
				if (!UCRTuningSettings::FindItem(Name))
				{
					Warnings.Add(FString::Printf(TEXT("%s: 튜닝 항목 %s 없음"), *File, *Name.ToString()));
				}
				else if (Field.Value->TryGetNumber(Number))
				{
					OutTuning.Add(Name, static_cast<float>(Number));
				}
				else if (Field.Value->TryGetBool(bBool))
				{
					OutTuning.Add(Name, bBool ? 1.f : 0.f);
				}
				else
				{
					Warnings.Add(FString::Printf(TEXT("%s: %s 는 숫자여야 함"), *File, *Name.ToString()));
				}
			}
		}
	}

	// 업그레이드
	const TArray<TSharedPtr<FJsonValue>>* UpgradeArray = nullptr;
	if (Root->TryGetArrayField(TEXT("upgrades"), UpgradeArray))
	{
		for (int32 i = 0; i < UpgradeArray->Num(); ++i)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!(*UpgradeArray)[i]->TryGetObject(Object))
			{
				continue;
			}
			FCRUpgradeDef Def;
			FString Id, Skill, Requires;
			(*Object)->TryGetStringField(TEXT("id"), Id);
			(*Object)->TryGetStringField(TEXT("skill"), Skill);
			if (Id.IsEmpty() || !CRSkill::ParseSkillKey(Skill, Def.Family, Def.Form))
			{
				Warnings.Add(FString::Printf(TEXT("%s: upgrades[%d] id 또는 skill(예: active.sword) 잘못됨"), *File, i));
				continue;
			}
			Def.Id = FName(*Id);
			if (FindUpgrade(Def.Id))
			{
				Warnings.Add(FString::Printf(TEXT("%s: 업그레이드 id %s 중복"), *File, *Id));
				continue;
			}
			(*Object)->TryGetStringField(TEXT("name"), Def.Name);
			(*Object)->TryGetStringField(TEXT("desc"), Def.Desc);
			if ((*Object)->TryGetStringField(TEXT("requires"), Requires))
			{
				Def.Requires = FName(*Requires);
			}
			double MaxLevel = 1.0;
			(*Object)->TryGetNumberField(TEXT("max"), MaxLevel);
			Def.MaxLevel = FMath::Clamp(FMath::RoundToInt(MaxLevel), 1, MaxUpgradeLevel);
			if (Def.Name.IsEmpty())
			{
				Def.Name = Id;
			}

			const TArray<TSharedPtr<FJsonValue>>* Mods = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("mods"), Mods))
			{
				for (const TSharedPtr<FJsonValue>& ModValue : *Mods)
				{
					const TSharedPtr<FJsonObject>* ModObject = nullptr;
					FString StatName;
					FCRStatMod Mod;
					if (!ModValue->TryGetObject(ModObject) || !(*ModObject)->TryGetStringField(TEXT("stat"), StatName)
						|| !CRSkill::FindStat(StatName, Mod.Stat))
					{
						Warnings.Add(FString::Printf(TEXT("%s: 업그레이드 %s 의 mods 항목 stat 이름 잘못됨"), *File, *Id));
						continue;
					}
					double Add = 0.0, Mul = 1.0;
					(*ModObject)->TryGetNumberField(TEXT("add"), Add);
					(*ModObject)->TryGetNumberField(TEXT("mul"), Mul);
					Mod.Add = static_cast<float>(Add);
					Mod.Mul = FMath::Max(static_cast<float>(Mul), 0.f);
					Def.Mods.Add(Mod);
				}
			}
			Upgrades.Add(MoveTemp(Def));
		}
	}

	// 투구
	const TArray<TSharedPtr<FJsonValue>>* HelmetArray = nullptr;
	if (Root->TryGetArrayField(TEXT("helmets"), HelmetArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *HelmetArray)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			FString Id, Shape;
			if (!Value->TryGetObject(Object) || !(*Object)->TryGetStringField(TEXT("id"), Id) || !Id.StartsWith(TEXT("helmet.")))
			{
				Warnings.Add(FString::Printf(TEXT("%s: helmets 항목 id는 helmet.으로 시작해야 함"), *File));
				continue;
			}
			FCRHelmetDef Def;
			Def.Id = FName(*Id);
			(*Object)->TryGetStringField(TEXT("name"), Def.Name);
			(*Object)->TryGetStringField(TEXT("shape"), Shape);
			Def.Shape = Shape.IsEmpty() ? FName(TEXT("cap")) : FName(*Shape);
			Def.Color = ReadColor(*Object, TEXT("color"), FLinearColor(0.02f, 0.02f, 0.025f));
			Helmets.Add(MoveTemp(Def));
		}
	}

	// 블럭 표: { "blocks": { "stone": { "health": 4, "color": [r, g, b] } } }
	const TSharedPtr<FJsonObject>* BlockObject = nullptr;
	if (Root->TryGetObjectField(TEXT("blocks"), BlockObject))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*BlockObject)->Values)
		{
			const ECRBlock Block = CRMapFile::FindBlock(Pair.Key);
			const TSharedPtr<FJsonObject>* Fields = nullptr;
			if (Block == ECRBlock::Count || Block == ECRBlock::Empty || !Pair.Value->TryGetObject(Fields))
			{
				Warnings.Add(FString::Printf(TEXT("%s: 블럭 %s 없음"), *File, *Pair.Key));
				continue;
			}
			double Health = CRMapFile::GetBlockHealth(Block);
			(*Fields)->TryGetNumberField(TEXT("health"), Health);
			CRMapFile::SetBlockOverride(Block, FMath::RoundToInt(Health), ReadColor(*Fields, TEXT("color"), CRMapFile::GetBlockColor(Block)));
		}
	}
}
