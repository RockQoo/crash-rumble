#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SBTuningSubsystem.generated.h"

class USBTuningSettings;

/** 튜닝 값 저장본. 항목 이름 → 값. 레지스트리에서 빠진 항목은 무시된다. */
UCLASS()
class SNOWBATTLE_API USBTuningSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FName, float> Values;
};

DECLARE_MULTICAST_DELEGATE(FSBOnTuningChanged);

/** 런타임 튜닝 값을 들고 있고, 로드/저장/초기화와 콘솔 명령(sb.Tune)을 담당한다. */
UCLASS()
class SNOWBATTLE_API USBTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USBTuningSubsystem* Get(const UObject* WorldContext);

	/** 어디서든 현재 튜닝 값 조회. 서브시스템이 없으면 코드 기본값. */
	static const USBTuningSettings& GetSettings(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USBTuningSettings* GetMutableSettings() const { return Settings; }

	void SetValue(FName Name, float Value);
	void ResetToDefaults();
	void Save();
	void NotifyChanged() { OnChanged.Broadcast(); }

	FSBOnTuningChanged OnChanged;

private:
	void Load();
	void HandleTuneCommand(const TArray<FString>& Args);

	UPROPERTY()
	TObjectPtr<USBTuningSettings> Settings;

	IConsoleObject* TuneCommand = nullptr;
};
