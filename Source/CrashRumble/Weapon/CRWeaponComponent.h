#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/CRSkill.h"
#include "Weapon/CRWeaponTypes.h"
#include "CRWeaponComponent.generated.h"

class ACRCharacter;
class UGameplayAbility;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FCRSwingHitDelegate, const FHitResult&);

USTRUCT()
struct FCRWeaponVisual
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** 무기 루트(손) 기준 총구 위치. */
	FVector MuzzleOffset = FVector::ZeroVector;
	FLinearColor FlashColor = FLinearColor::White;
};

/**
 * 오른손에 드는 무기. 외형(기본 도형), 손 포즈(칼 휘두르기 / 총 조준), 발사 간격,
 * 반동(스프링 킥백 + 연사 떨림), 총구 화염, 레이저 빔 표시를 담당한다.
 * 수치는 로드아웃의 스킬 스탯(튜닝 기본값 + 업그레이드)을 읽는다.
 * Shoot* 함수는 시작점·방향을 직접 받아 패시브(자동 발동)도 같이 쓴다. 피해는 CRCombat이 적용.
 */
UCLASS(ClassGroup = (CrashRumble), meta = (BlueprintSpawnableComponent))
class CRASHRUMBLE_API UCRWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCRWeaponComponent();

	/** 캐릭터가 파츠를 조립한 뒤 호출. Hand = 무기를 붙일 손, OffHand = 쌍검 왼손. */
	void Initialize(USceneComponent* InHand, USceneComponent* InOffHand = nullptr);

	void Equip(ECRWeaponType Type);
	ECRWeaponType GetEquipped() const { return Equipped; }

	/** 무기 메시들의 부모 (손에 붙어 있음). 사망 시 통째로 떼어 날린다. */
	USceneComponent* GetWeaponRoot() const { return WeaponRoot; }

	void SetTriggerHeld(bool bHeld) { bTriggerHeld = bHeld; }
	bool IsTriggerHeld() const { return bTriggerHeld; }

	// 어빌리티에서 호출 (액티브: 총구·조준점 기준 + 반동·총구 화염)
	void StartSwing(FCRSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished);
	void FireLaser();
	void FireProjectile();
	void FireLightning();
	/** 런처: 업그레이드된 발 수 (CountOverride >= 0이면 그 수, bSuper면 필살기 일제 사격). */
	void FireMissiles(int32 CountOverride = -1, bool bSuper = false);
	void FireFlame();
	FVector GetMuzzleLocation() const;

	// 시작점·방향을 받는 공용 발사 (패시브도 사용)
	void ShootLaser(const FVector& Start, const FVector& Direction, ECRSkillForm Form);
	void ShootProjectiles(const FVector& Start, const FVector& Direction, ECRSkillForm Form);
	/** 번개 줄기 + 체인 (Start → AimPoint 방향, 사거리까지). */
	void ShootLightning(const FVector& Start, const FVector& AimPoint, ECRSkillForm Form);
	/** 조준 지점으로 발 수만큼 (유도면 근처 적, 일제 사격이면 부채꼴 궤적, 아니면 빨간 범위 예고 후 그 지점). */
	void ShootMissiles(const FVector& AimPoint, ECRSkillForm Form, int32 CountOverride = -1, bool bSuper = false);
	/** 조준 지점 바닥부터 Direction 쪽으로 칸 수만큼 불꽃 + 반경 피해 + 화상. */
	void ShootFlame(const FVector& AimPoint, const FVector& Direction, ECRSkillForm Form);

	/** 필살기 준비 자세처럼 오른손 포즈를 Duration(오너 시간)초 동안 덮어쓴다. */
	void SetHandOverride(const FVector& Location, const FRotator& Rotation, float Duration);

	/** 툰 셰이딩 토글에 맞춰 무기 표면 머티리얼을 바꾼다 (발광 파츠는 그대로). */
	void RefreshSurfaceMaterial();

	/** 오른손 포즈 (캡슐 기준). false면 기본 자세. */
	bool GetHandPose(FVector& OutLocation, FRotator& OutRotation) const;
	/** 쌍검 왼손 포즈 (오른손을 좌우로 뒤집은 것). false면 왼손은 평소대로. */
	bool GetOffHandPose(FVector& OutLocation, FRotator& OutRotation) const;

	/** 업그레이드 등으로 스탯이 바뀌면 칼 길이·쌍검 외형을 다시 맞춘다. */
	void RefreshFromStats();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FBeamFx
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Width = 4.f;
		float TimeLeft = 0.f;
	};

	struct FPendingMissile
	{
		TWeakObjectPtr<AActor> Target;
		FVector TargetPoint = FVector::ZeroVector;
		ECRSkillForm Form = ECRSkillForm::Active;
		bool bSalvo = false;
		int32 SalvoIndex = 0;
		int32 SalvoSize = 1;
		float LaunchTime = 0.f;
	};

	void BuildVisuals();
	/** 이 무기 외형을 아직 안 만들었으면 만든다 (장착할 때). */
	void BuildVisual(ECRWeaponType Type);
	void BuildOffHandVisual();
	UStaticMeshComponent* MakePart(USceneComponent* Parent, const TCHAR* MeshPath, const FVector& Location,
		const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color, bool bGlow, float GlowIntensity = 1.f);

	FCRSkillStats GetStats(ECRWeaponType Family, ECRSkillForm Form) const;
	bool IsDualActive() const;

	void TryFire();
	void UpdateSwing(float DeltaTime);
	void SweepBlade(USceneComponent* Root, TArray<FVector>& PrevSamples, bool bFirstSample);
	void UpdateHandPose(float DeltaTime);
	void UpdateRecoil(float DeltaTime);
	void UpdateEffects(float RealDeltaTime);
	void UpdateSwordBlade();

	void PlayFireFeedback();
	FVector GetFireDirection(const FVector& Start) const;
	FVector GetAimPoint() const;
	void UpdateMissileQueue();
	void LaunchMissile(const FPendingMissile& Pending);
	void AddBeam(const FVector& Start, const FVector& End, float Width);
	TSubclassOf<UGameplayAbility> GetAbilityClass(ECRWeaponType Type) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> WeaponRoot;

	/** 쌍검 왼손 칼 (업그레이드로 켬). */
	UPROPERTY()
	TObjectPtr<USceneComponent> OffHandRoot;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OffHandParts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> OffHandBlade;

	UPROPERTY()
	TMap<ECRWeaponType, FCRWeaponVisual> Visuals;

	/** 발광이 아닌 무기 파츠 (툰 / 조명 머티리얼 전환 대상). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SurfaceParts;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SwordBlade;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> MuzzleLight;

	/** 레이저 빔 메시 (여러 줄기·패시브, 처음 필요할 때 늘림). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BeamMeshes;

	TArray<FBeamFx> Beams;

	ECRWeaponType Equipped = ECRWeaponType::None;
	bool bTriggerHeld = false;

	/** 오너 시간 배율(히트스톱)을 따르는 시계. 발사 간격 계산용. */
	float WeaponClock = 0.f;
	float NextFireTime = 0.f;

	// 칼 휘두르기
	bool bSwinging = false;
	float SwingTime = 0.f;
	float RecoverTime = 0.f;
	float SwingDuration = 0.18f;
	float SwingArc = 150.f;
	float BladeLength = 90.f;
	FCRSwingHitDelegate OnSwingHit;
	FSimpleDelegate OnSwingFinished;
	TArray<FVector> PrevBladeSamples;
	TArray<FVector> PrevOffBladeSamples;
	TSet<TWeakObjectPtr<AActor>> SwingHitActors;

	// 손 포즈
	FVector HandLocation = FVector::ZeroVector;
	FRotator HandRotation = FRotator::ZeroRotator;
	bool bHasHandPose = false;

	// 반동 스프링 (1 = 한 발 킥)
	float Recoil = 0.f;
	float RecoilVelocity = 0.f;
	FVector RecoilJitterOffset = FVector::ZeroVector;

	// 미사일 대기열 (오너 시간)
	TArray<FPendingMissile> PendingMissiles;

	// 손 포즈 덮어쓰기 (필살기)
	FVector OverrideHandLocation = FVector::ZeroVector;
	FRotator OverrideHandRotation = FRotator::ZeroRotator;
	float OverrideTimeLeft = 0.f;

	// 이펙트 (실시간 기준)
	float MuzzleFlashTimeLeft = 0.f;
};
