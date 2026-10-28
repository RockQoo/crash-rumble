#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SBPlayerController.generated.h"

class SSBTuningPanel;
class UInputAction;
class UInputMappingContext;
class SWidget;
enum class ESBViewMode : uint8;
struct FInputActionValue;

/**
 * 입력(Enhanced Input을 C++에서 직접 구성), 시점별 커서/입력 모드, Tab 옵션창을 담당.
 */
UCLASS()
class SNOWBATTLE_API ASBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void ToggleOptions();
	bool IsOptionsOpen() const { return OptionsPanel.IsValid(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	void CreateInputAssets();
	void RefreshInputMode();
	void HandleViewModeChanged(ESBViewMode NewMode);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnToggleView();
	void OnToggleOptions();
	void OnFirePressed();
	void OnFireReleased();
	void OnSelectSword();
	void OnSelectLaser();
	void OnSelectProjectile();
	void SetTriggerHeld(bool bHeld);

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleViewAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> OptionsAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SwordAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LaserAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ProjectileAction;

	TSharedPtr<SSBTuningPanel> OptionsPanel;
	TSharedPtr<SWidget> HelpOverlay;
};
