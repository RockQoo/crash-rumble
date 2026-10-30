#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CRTuningSubsystem.generated.h"

class UCRTuningSettings;

/** 튜닝 값 저장본. 기본값과 다른 항목만 이름 → 값으로 저장. 레지스트리에서 빠진 항목은 무시된다. */
UCLASS()
class CRASHRUMBLE_API UCRTuningSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	TMap<FName, float> Values;
};

DECLARE_MULTICAST_DELEGATE(FCROnTuningChanged);

/** 런타임 튜닝 값을 들고 있고, 로드/저장/초기화와 콘솔 명령(cr.Tune)을 담당한다. */
UCLASS()
class CRASHRUMBLE_API UCRTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UCRTuningSubsystem* Get(const UObject* WorldContext);

	/** 어디서든 현재 튜닝 값 조회. 서브시스템이 없으면 코드 기본값. */
	static const UCRTuningSettings& GetSettings(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UCRTuningSettings* GetMutableSettings() const { return Settings; }

	void SetValue(FName Name, float Value);
	void ResetToDefaults();
	void Save();
	void NotifyChanged() { OnChanged.Broadcast(); }

	/**
	 * GameData JSON의 기본값 (항목 이름 → 값). 비어 있는 항목은 코드 기본값.
	 * 저장된 값(사용자가 바꾼 값)이 없는 항목만 새 기본값을 따른다. 저장/초기화 비교 기준도 이 기본값.
	 */
	void ApplyJsonDefaults(const TMap<FName, float>& Values);
	const UCRTuningSettings* GetDefaults() const { return Defaults; }

	FCROnTuningChanged OnChanged;

private:
	void Load();
	void HandleTuneCommand(const TArray<FString>& Args);

	UPROPERTY()
	TObjectPtr<UCRTuningSettings> Settings;

	/** 코드 기본값 + GameData JSON. */
	UPROPERTY()
	TObjectPtr<UCRTuningSettings> Defaults;

	/** 저장본에서 불러온(사용자가 바꾼) 항목. JSON 기본값이 바뀌어도 덮어쓰지 않는다. */
	TSet<FName> SavedNames;

	IConsoleObject* TuneCommand = nullptr;
};
