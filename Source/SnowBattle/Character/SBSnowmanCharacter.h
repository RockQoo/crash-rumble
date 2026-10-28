#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "SBSnowmanCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class USBAttributeSet;
class USBSnowmanAppearance;
class USBViewComponent;
class USBWeaponComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

/**
 * 스태틱 메시 파츠로 조립하는 눈사람. 충돌은 캡슐 하나.
 * 플레이어와 허수아비가 같은 클래스를 쓴다 (허수아비는 컨트롤러 입력이 없을 뿐).
 */
UCLASS()
class SNOWBATTLE_API ASBSnowmanCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASBSnowmanCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 이동 입력. X = 오른쪽, Y = 앞 (카메라 기준). */
	void MoveInput(const FVector2D& Axis);
	void LookInput(const FVector2D& Axis);
	void ToggleViewMode();

	USBViewComponent* GetViewComponent() const { return ViewComponent; }
	USBWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	UStaticMeshComponent* FindPart(FName Id) const;

	float GetHealth() const;
	float GetMaxHealth() const;
	bool IsDead() const { return bDead; }

	// 타격감
	void PlayHitFlash(float Duration, float Intensity);
	void ApplyKnockback(const FVector& Velocity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비우면 기본 도형 눈사람. */
	UPROPERTY(EditAnywhere, Category = "Snowman")
	TObjectPtr<USBSnowmanAppearance> Appearance;

	/** 모자/손 색. 플레이어와 허수아비 구분용. */
	UPROPERTY(EditAnywhere, Category = "Snowman")
	FLinearColor AccentColor = FLinearColor(0.1f, 0.35f, 1.f);

	/** 파츠 부모. 쓰러질 때 이걸 바닥 기준으로 기울인다. */
	UPROPERTY(VisibleAnywhere, Category = "Snowman")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USBViewComponent> ViewComponent;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<USBWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<USBAttributeSet> Attributes;

private:
	void BuildParts();
	void ApplyTuning();
	void InitAbilitySystem();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void Die();
	void Revive();

	void UpdateHands(float DeltaSeconds);
	void UpdateFacing(float DeltaSeconds);
	void UpdateFlash(float DeltaSeconds);
	void UpdateFall(float DeltaSeconds);
	void SetPartsFlash(float Value);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	TMap<FName, FVector> HandRestLocations;
	float HandBobTime = 0.f;
	FDelegateHandle TuningChangedHandle;
	bool bUseCustomPrimitiveData = false;
	bool bAbilitiesGranted = false;

	// 쓰러짐: 0 = 서 있음, 1 = 완전히 누움
	bool bDead = false;
	float FallAlpha = 0.f;
	FVector FallAxis = FVector::RightVector;
	FTimerHandle ReviveTimer;

	float FlashTimeLeft = 0.f;
	float FlashDuration = 0.f;
	float FlashIntensity = 0.f;
};
