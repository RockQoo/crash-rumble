#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Core/CRActorPool.h"
#include "GameFramework/Character.h"
#include "Weapon/CRWeaponTypes.h"
#include "CRCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UCRAttributeSet;
class UCRCharacterAppearance;
class UCRViewComponent;
class UCRWeaponComponent;
class UMaterialInterface;
class USpringArmComponent;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

/** 팀 전투용. None은 플레이어/일반 몬스터. */
UENUM()
enum class ECRTeam : uint8
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
class CRASHRUMBLE_API ACRCharacter : public ACharacter, public IAbilitySystemInterface, public ICRPoolable
{
	GENERATED_BODY()

public:
	ACRCharacter();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	virtual void Landed(const FHitResult& Hit) override;

	// 풀 (다수 타격 테스트 몬스터)
	virtual void OnPoolAcquired() override;
	virtual void OnPoolReleased() override;

	/**
	 * 풀에서 꺼낸 몬스터로 설정: 색 변경, 죽으면 하늘에서 재등장.
	 * 팀이 없으면 플레이어를 쫓고, 팀이 있으면 가장 가까운 적 팀을 찾아 칼로 공격한다.
	 */
	void SetupAsMonster(const FLinearColor& InAccentColor, ECRTeam InTeam = ECRTeam::None,
		ECRWeaponType InWeapon = ECRWeaponType::Sword);
	bool IsMonster() const { return bMonster; }
	bool IsInPool() const { return bInPool; }
	ECRTeam GetTeam() const { return Team; }
	void SetAccentColor(const FLinearColor& InAccentColor);
	const FLinearColor& GetAccentColor() const { return AccentColor; }

	/** 관전: 숨기고 다른 눈사람과 부딪히지 않게 한다. */
	void SetSpectating(bool bInSpectating);

	/** 무기가 겨눌 지점. 플레이어는 조준점, 팀 전투 AI는 현재 적. 없으면 false. */
	bool GetWeaponAimPoint(FVector& OutPoint) const;

	/** 이동 입력. X = 오른쪽, Y = 앞 (카메라 기준). */
	void MoveInput(const FVector2D& Axis);
	void LookInput(const FVector2D& Axis);
	void ToggleViewMode();

	UCRViewComponent* GetViewComponent() const { return ViewComponent; }
	UCRWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	UStaticMeshComponent* FindPart(FName Id) const;

	float GetHealth() const;
	float GetMaxHealth() const;
	bool IsDead() const { return bDead; }

	/** 왼손 펀치: 커서 쪽으로 즉시 돌고 왼손을 한 프레임에 뻗은 뒤 주먹 위치에서 판정 (GA_Punch가 호출). */
	void PerformPunch();
	/** 우클릭: 간격이 지났으면 펀치 어빌리티 실행. */
	void TryPunch();
	/** F: 쿨다운이 지났으면 필살기 어빌리티 실행. */
	void TrySuper();
	void SetSprinting(bool bInSprinting);
	bool IsSprinting() const { return bSprinting; }

	// 타격감
	void PlayHitFlash(float Duration, float Intensity);
	void ApplyKnockback(const FVector& Velocity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 비우면 기본 도형 눈사람. */
	UPROPERTY(EditAnywhere, Category = "Snowman")
	TObjectPtr<UCRCharacterAppearance> Appearance;

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
	TObjectPtr<UCRViewComponent> ViewComponent;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UCRWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UCRAttributeSet> Attributes;

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
	void UpdatePawnBlocking();

	// 사망 연출: 몸은 맞은 방향으로 넘어지고, 투구(모자)와 무기가 벗겨져 튕긴다 (물리 엔진 없이 직접 계산)
	void StartDeathFx(const FVector& HitVelocity, bool bLaunchBody);
	void UpdatePopOff(float DeltaSeconds);
	void Reassemble();

	void UpdateHands(float DeltaSeconds);
	void UpdateFacing(float DeltaSeconds);
	void UpdateFlash(float DeltaSeconds);
	void UpdateFall(float DeltaSeconds);
	void SetPartsFlash(float Value);

	// 애니 연출 바탕 (10단계)
	/** 숨쉬기: 몸통이 위아래로 늘었다 줄고, 머리와 모자가 조금씩 늦게 따라온다. 걸을 때는 통통 튄다. */
	void UpdateBreath(float DeltaSeconds);
	void UpdateBlobShadow();
	/** 빠르게 움직인 손을 지난 그림과 이번 그림 사이에 늘린 구체로 채운다 (몸 기준 속도). */
	void UpdateSmear(float DeltaSeconds);
	/** 캐릭터 2코마: 모션은 매 프레임 계산하고, 그리지 않는 프레임엔 마지막 그림의 트랜스폼으로 되돌린다. */
	void ApplyCharStepping();
	void RestoreCharStepping();
	/** 툰 셰이딩 토글에 맞춰 파츠/무기 머티리얼과 그림자 캐스팅을 바꾼다. */
	void RefreshSurfaceMaterial();
	/** 플레이어는 스텐실 1 (임팩트 프레임 실루엣, 슈퍼 프리즈 컬러 유지). 계속 켜 둔다. */
	void RefreshStencil();

	enum class EBreathGroup : uint8
	{
		None,   // 손 등 (따로 움직임)
		Body,   // 몸통: 바닥을 기준으로 늘어남
		OnBody, // 단추: 몸통 표면을 따라감
		Head,   // 머리와 얼굴
		Hat,    // 모자: 머리보다 더 늦게
	};

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;
	TArray<FTransform> PartRestTransforms; // PartComponents와 같은 순서
	TArray<EBreathGroup> PartBreathGroups;  // PartComponents와 같은 순서

	void UpdateMoveSpeed();
	void UpdatePunch(float DeltaSeconds);

	// 펀치: 1 = 다 뻗음, 스프링으로 0에 돌아온다
	float PunchExtend = 0.f;
	float PunchVelocity = 0.f;
	float PunchClock = 0.f;
	float NextPunchTime = 0.f;
	bool bSprinting = false;
	double NextSuperTime = 0.0;

	float BreathTime = 0.f;
	float WalkPhase = 0.f;
	float WalkAlpha = 0.f;

	// 스미어: 마지막으로 그린 장의 손 위치 (몸 기준)
	TMap<FName, FVector> SmearHandLocations;
	int32 SmearDrawCount = -1;
	float SmearElapsed = 0.f;

	// 캐릭터 2코마
	FTransform StepRootWorld;
	FTransform StepRootMotion;
	TArray<FTransform> StepPartTransforms;
	bool bHasStepSnapshot = false;
	bool bStepApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	struct FDeathChunk
	{
		TArray<TWeakObjectPtr<USceneComponent>> Components;
		TArray<FTransform> Offsets;        // 조각 기준 상대 트랜스폼
		TArray<FTransform> RestTransforms; // 재조립 시 되돌릴 상대 트랜스폼
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinSpeed = 0.f; // 라디안/초
		float Radius = 10.f;
		bool bResting = false;
	};
	TArray<FDeathChunk> DeathChunks;
	bool bPoppedOff = false;
	bool bPendingDeathFx = false;
	bool bSpectating = false;
	float DeathGroundZ = 0.f;
	ECRWeaponType BattleWeapon = ECRWeaponType::Sword;

	TMap<FName, FVector> HandRestLocations;
	float HandBobTime = 0.f;
	FDelegateHandle TuningChangedHandle;
	bool bUseCustomPrimitiveData = false;
	bool bAbilitiesGranted = false;
	bool bMonster = false;
	bool bInPool = false;
	FVector HomeLocation = FVector::ZeroVector;

	ECRTeam Team = ECRTeam::None;
	TWeakObjectPtr<ACRCharacter> TargetEnemy;
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
