#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CRViewComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;

UENUM(BlueprintType)
enum class ECRViewMode : uint8
{
	Shoulder,
	TopDown
};

DECLARE_MULTICAST_DELEGATE_OneParam(FCROnViewModeChanged, ECRViewMode);

/**
 * 숄더뷰 ↔ 탑다운 카메라 전환과 시점별 조준점 계산.
 * 숄더뷰: 화면 중앙 조준 / 탑다운: 마우스 커서 지점 조준.
 */
UCLASS(ClassGroup = (CrashRumble), meta = (BlueprintSpawnableComponent))
class CRASHRUMBLE_API UCRViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCRViewComponent();

	void Setup(USpringArmComponent* InSpringArm, UCameraComponent* InCamera);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ToggleViewMode();
	ECRViewMode GetViewMode() const { return ViewMode; }

	/** 0 = 숄더뷰, 1 = 탑다운 (전환 중에는 중간값). */
	float GetBlendAlpha() const { return BlendAlpha; }

	/** 현재 조준점 (월드). */
	const FVector& GetAimPoint() const { return AimPoint; }

	/** 마우스 대신 지정한 지점을 조준 (자동 시연용). ClearAimOverride로 해제. */
	void SetAimOverride(const FVector& Point) { AimOverride = Point; bAimOverride = true; }
	void ClearAimOverride() { bAimOverride = false; }

	/** 이동 입력 기준이 되는 카메라 Yaw. */
	float GetMovementYaw() const;

	/** 캐릭터가 바라봐야 할 Yaw. 탑다운에서 커서가 너무 가까우면 false. */
	bool GetDesiredFacingYaw(float& OutYaw) const;

	/** 카메라 흔들림 (Scale은 튜닝 세기에 곱해짐). */
	void AddShake(float Scale);

	/** 카메라 반동: 위로 튀었다가 복귀. */
	void AddRecoil(float PitchDegrees);

	FCROnViewModeChanged OnViewModeChanged;

private:
	void UpdateCamera(float DeltaTime);
	void UpdateCameraOffset(float RealDeltaTime);
	void UpdateAim();
	FRotator GetControlRotation() const;

	float ShakeTimeLeft = 0.f;
	float ShakeAmplitude = 0.f;
	float ShakeClock = 0.f;
	float RecoilPitchOffset = 0.f;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	ECRViewMode ViewMode = ECRViewMode::Shoulder;
	float BlendAlpha = 0.f;
	float TopDownYaw = 0.f;
	FVector AimPoint = FVector::ZeroVector;
	FVector AimOverride = FVector::ZeroVector;
	bool bAimOverride = false;
};
