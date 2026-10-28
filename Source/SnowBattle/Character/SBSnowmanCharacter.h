#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Core/SBActorPool.h"
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

/** 팀 전투용. None은 플레이어/일반 몬스터. */
UENUM()
enum class ESBTeam : uint8
{
	None,
	Red,
	Blue,
};

/**
 * 스태틱 메시 파츠로 조립하는 눈사람. 충돌은 캡슐 하나.
 * 플레이어와 허수아비가 같은 클래스를 쓴다 (허수아비는 컨트롤러 입력이 없을 뿐).
 */
UCLASS()
class SNOWBATTLE_API ASBSnowmanCharacter : public ACharacter, public IAbilitySystemInterface, public ISBPoolable
{
	GENERATED_BODY()

public:
	ASBSnowmanCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	// 풀 (다수 타격 테스트 몬스터)
	virtual void OnPoolAcquired() override;
	virtual void OnPoolReleased() override;

	/**
	 * 풀에서 꺼낸 몬스터로 설정: 색 변경, 죽으면 하늘에서 재등장.
	 * 팀이 없으면 플레이어를 쫓고, 팀이 있으면 가장 가까운 적 팀을 찾아 칼로 공격한다.
	 */
	void SetupAsMonster(const FLinearColor& InAccentColor, ESBTeam InTeam = ESBTeam::None);
	bool IsMonster() const { return bMonster; }
	bool IsInPool() const { return bInPool; }
	ESBTeam GetTeam() const { return Team; }
	void SetAccentColor(const FLinearColor& InAccentColor);

	/** 관전: 숨기고 다른 눈사람과 부딪히지 않게 한다. */
	void SetSpectating(bool bSpectating);

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
	void RestoreFullHealth();
	void ResetPose();
	void Respawn(); // 몬스터: 하늘에서 / 그 외: 처음 위치로
	void UpdateChase(float DeltaSeconds);
	void UpdateBattle(float DeltaSeconds);
	float GetTunedMaxHealth() const;

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
	bool bMonster = false;
	bool bInPool = false;
	FVector HomeLocation = FVector::ZeroVector;

	ESBTeam Team = ESBTeam::None;
	TWeakObjectPtr<ASBSnowmanCharacter> TargetEnemy;
	float RetargetTimeLeft = 0.f;

	// 쓰러짐: 0 = 서 있음, 1 = 완전히 누움
	bool bDead = false;
	float FallAlpha = 0.f;
	FVector FallAxis = FVector::RightVector;
	FTimerHandle ReviveTimer;

	float FlashTimeLeft = 0.f;
	float FlashDuration = 0.f;
	float FlashIntensity = 0.f;
};
