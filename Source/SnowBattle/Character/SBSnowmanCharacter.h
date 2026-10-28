#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SBSnowmanCharacter.generated.h"

class UCameraComponent;
class USBSnowmanAppearance;
class USBViewComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * 스태틱 메시 파츠로 조립하는 눈사람. 충돌은 캡슐 하나.
 * 플레이어와 허수아비가 같은 클래스를 쓴다 (허수아비는 컨트롤러 입력이 없을 뿐).
 */
UCLASS()
class SNOWBATTLE_API ASBSnowmanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASBSnowmanCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 이동 입력. X = 오른쪽, Y = 앞 (카메라 기준). */
	void MoveInput(const FVector2D& Axis);
	void LookInput(const FVector2D& Axis);
	void ToggleViewMode();

	USBViewComponent* GetViewComponent() const { return ViewComponent; }
	UStaticMeshComponent* FindPart(FName Id) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비우면 기본 도형 눈사람. */
	UPROPERTY(EditAnywhere, Category = "Snowman")
	TObjectPtr<USBSnowmanAppearance> Appearance;

	/** 모자/손 색. 플레이어와 허수아비 구분용. */
	UPROPERTY(EditAnywhere, Category = "Snowman")
	FLinearColor AccentColor = FLinearColor(0.1f, 0.35f, 1.f);

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USBViewComponent> ViewComponent;

private:
	void BuildParts();
	void ApplyTuning();
	void UpdateHands(float DeltaSeconds);
	void UpdateFacing(float DeltaSeconds);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	TMap<FName, FVector> HandRestLocations;
	float HandBobTime = 0.f;
	FDelegateHandle TuningChangedHandle;
};
