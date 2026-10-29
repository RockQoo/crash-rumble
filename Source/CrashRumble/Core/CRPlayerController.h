#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRPlayerController.generated.h"

class SCRTuningPanel;
class UInputAction;
class UInputMappingContext;
class SWidget;
enum class ECRViewMode : uint8;
enum class ECRWeaponType : uint8;
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

	/** Weapon: 1 칼, 2 레이저, 3 투사체, 4 번개, 5 미사일, 6 불꽃. Seconds 동안 공격 버튼을 누르고 있는다. */
	UFUNCTION(Exec)
	void CRAutoFire(int32 Weapon, float Seconds);

	/** 우클릭 펀치 (녹화/자동 테스트용). */
	UFUNCTION(Exec)
	void CRPunch();

	/** F 필살기 (녹화/자동 테스트용). */
	UFUNCTION(Exec)
	void CRSuper();

	/** 옵션창을 Tab번째 탭으로 열고 닫는다 (녹화/자동 확인용, -1 = 지금 탭). */
	UFUNCTION(Exec)
	void CROptions(int32 Tab);

	/** 옵션창 탭 이름의 효과를 플레이어 앞에 터뜨린다 (예: CRPreview 구체 폭발). */
	UFUNCTION(Exec)
	void CRPreview(const FString& Category);

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

	void SelectWeapon(ECRWeaponType Type);
	void OnSelectLightning();
	void OnSelectMissile();
	void OnSelectFlame();
	void OnPunch();
	void OnSuper();
	/** T / Y / I: 이펙트 2코마, 캐릭터 2코마, 임팩트 프레임 켜고 끄기 (튜닝 값을 바꾸고 화면에 잠깐 표시). */
	void OnToggleFxStep();
	void OnToggleCharStep();
	void OnToggleImpact();
	void ShowToast(const FString& Message);
	/** 맵 제작 모드: 화면 가운데 블럭에 피해 (bCrater면 반경 지우기). */
	void EditBlocks(bool bCrater);

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LightningAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MissileAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FlameAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PunchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SuperAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FxStepAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CharStepAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ImpactAction;

	FText ToastText;
	double ToastEnd = 0.0;

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
