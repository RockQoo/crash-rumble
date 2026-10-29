#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRPlayerController.generated.h"

class SCRTuningPanel;
class UInputAction;
class UInputMappingContext;
class SWidget;
enum class ECRViewMode : uint8;
struct FInputActionValue;

/**
 * 입력(Enhanced Input을 C++에서 직접 구성), 시점별 커서/입력 모드, Tab 옵션창을 담당.
 */
UCLASS()
class CRASHRUMBLE_API ACRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void ToggleOptions();
	bool IsOptionsOpen() const { return OptionsPanel.IsValid(); }

	/** 화면 왼쪽 위 통계: FPS, 몬스터 수, 총알 수, 풀 생성 수. */
	FText GetStatsText();

	// 테스트용 콘솔 명령 (Shipping 제외)
	UFUNCTION(Exec)
	void CRSpawn(int32 Count);

	UFUNCTION(Exec)
	void CRClear();

	/** 레드 vs 블루 팀 전투 시작 (팀당 PerTeam 마리). */
	UFUNCTION(Exec)
	void CRBattle(int32 PerTeam);

	/** V 키와 같음 (녹화/자동 테스트용). */
	UFUNCTION(Exec)
	void CRToggleView();

	/**
	 * 타격감 시연(녹화용): 가장 가까운 몬스터를 자동 조준해 공격하며
	 * SecondsPerWeapon마다 칼 → 레이저 → 투사체로 바꾼다. 0이면 끈다.
	 */
	UFUNCTION(Exec)
	void CRDemo(float SecondsPerWeapon);

	virtual void PlayerTick(float DeltaTime) override;

	/** Weapon: 1 칼, 2 레이저, 3 투사체. Seconds 동안 공격 버튼을 누르고 있는다. */
	UFUNCTION(Exec)
	void CRAutoFire(int32 Weapon, float Seconds);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	void CreateInputAssets();
	void RefreshInputMode();
	void HandleViewModeChanged(ECRViewMode NewMode);

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

	/** Esc: 옵션창이 열려 있으면 닫고, 아니면 타이틀로. (에디터 PIE에서는 Esc가 플레이 종료라 옵션창 버튼을 쓴다) */
	void OnBack();
	void GoToTitle();

	// 맵 제작 모드 자유 카메라 (폰이 캐릭터가 아닐 때)
	void OnFly(const FInputActionValue& Value);
	void OnFastPressed();
	void OnFastReleased();
	FText GetHelpText() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlyAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FastAction;

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

	TSharedPtr<SCRTuningPanel> OptionsPanel;
	TSharedPtr<SWidget> HelpOverlay;
	FTimerHandle AutoFireTimer;
	float DemoWeaponSeconds = 0.f;
	float DemoTime = 0.f;
	int32 DemoWeaponIndex = -1;
	float SmoothedFrameMs = 0.f;
};
