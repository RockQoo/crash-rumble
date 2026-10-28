#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SBTuningSettings.generated.h"

/** 옵션창 한 줄. 레지스트리(GetItems)에 등록된 항목만 옵션창/저장/콘솔에 노출된다. */
struct FSBTuningItem
{
	FText Category;
	FText Label;
	FProperty* Property = nullptr; // float / int32 / bool
	float Min = 0.f;
	float Max = 1.f;
	float Step = 0.f;              // 0이면 스핀박스 기본 이동량

	FName GetName() const { return Property ? Property->GetFName() : NAME_None; }
	float GetValue(const void* Container) const;
	void SetValue(void* Container, float Value) const;
	bool IsBool() const;
};

/**
 * 인게임에서 조정 가능한 모든 튜닝 수치.
 * 기본값은 여기 멤버 초기값이고, 옵션창에서 바꾼 값은 USBTuningSubsystem이 SaveGame으로 저장한다.
 * 새 수치 추가: 멤버 추가 → SBTuningSettings.cpp의 GetItems()에 SB_TUNE 한 줄.
 */
UCLASS()
class SNOWBATTLE_API USBTuningSettings : public UObject
{
	GENERATED_BODY()

public:
	static const TArray<FSBTuningItem>& GetItems();
	static const FSBTuningItem* FindItem(FName Name);

	// 이동
	UPROPERTY() float MoveSpeed = 600.f;
	UPROPERTY() float MaxAcceleration = 4096.f;
	UPROPERTY() float TurnSpeed = 14.f;

	// 카메라 - 공통
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() bool bInvertMouseY = false;
	UPROPERTY() float ViewBlendSpeed = 6.f;

	// 카메라 - 숄더뷰
	UPROPERTY() float ShoulderArmLength = 320.f;
	UPROPERTY() float ShoulderOffsetY = 70.f;
	UPROPERTY() float ShoulderOffsetZ = 70.f;
	UPROPERTY() float ShoulderFOV = 90.f;

	// 카메라 - 탑다운
	UPROPERTY() float TopDownArmLength = 1500.f;
	UPROPERTY() float TopDownPitch = 60.f;
	UPROPERTY() float TopDownFOV = 70.f;

	// 손
	UPROPERTY() float HandBobAmplitude = 4.f;
	UPROPERTY() float HandBobSpeed = 2.f;
};
