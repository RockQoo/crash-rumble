#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/CRWeaponTypes.h"
#include "CRWeaponComponent.generated.h"

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
 * 데미지 적용은 GAS 어빌리티(CRWeaponAbilities)가 한다.
 */
UCLASS(ClassGroup = (CrashRumble), meta = (BlueprintSpawnableComponent))
class CRASHRUMBLE_API UCRWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCRWeaponComponent();

	/** 캐릭터가 파츠를 조립한 뒤 호출. Hand = 무기를 붙일 손. */
	void Initialize(USceneComponent* InHand);

	void Equip(ECRWeaponType Type);
	ECRWeaponType GetEquipped() const { return Equipped; }

	/** 무기 메시들의 부모 (손에 붙어 있음). 사망 시 통째로 떼어 날린다. */
	USceneComponent* GetWeaponRoot() const { return WeaponRoot; }

	void SetTriggerHeld(bool bHeld) { bTriggerHeld = bHeld; }

	// 어빌리티에서 호출
	void StartSwing(FCRSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished);
	/** 맞힌 눈사람 목록을 채우고 개수를 돌려준다 (관통 수 반영). */
	int32 FireLaser(TArray<FHitResult>& OutTargetHits);
	void FireProjectile();
	/** 번개 한 줄기. 맞힌 눈사람 (없으면 빈 배열). */
	void FireLightning(TArray<FHitResult>& OutTargetHits);
	/** 미사일 Count발을 간격을 두고 차례로 쏜다 (bSuper면 간격 절반). */
	void FireMissiles(int32 Count, bool bSuper = false);
	/** 조준 지점 바닥에 폴리곤 불꽃. 불꽃 중심(바닥)을 돌려준다. */
	FVector FireFlame();
	FVector GetMuzzleLocation() const;

	/** 필살기 준비 자세처럼 오른손 포즈를 Duration(오너 시간)초 동안 덮어쓴다. */
	void SetHandOverride(const FVector& Location, const FRotator& Rotation, float Duration);

	/** 툰 셰이딩 토글에 맞춰 무기 표면 머티리얼을 바꾼다 (발광 파츠는 그대로). */
	void RefreshSurfaceMaterial();

	/** 오른손 포즈 (캡슐 기준). false면 기본 자세. */
	bool GetHandPose(FVector& OutLocation, FRotator& OutRotation) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildVisuals();
	UStaticMeshComponent* MakePart(USceneComponent* Parent, const TCHAR* MeshPath, const FVector& Location,
		const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color, bool bGlow, float GlowIntensity = 1.f);

	void TryFire();
	void UpdateSwing(float DeltaTime);
	void SweepBlade(bool bFirstSample);
	void UpdateHandPose(float DeltaTime);
	void UpdateRecoil(float DeltaTime);
	void UpdateEffects(float RealDeltaTime);
	void UpdateSwordBlade();

	void PlayFireFeedback();
	FVector GetFireDirection(const FVector& Start) const;
	void UpdateMissileQueue();
	void LaunchOneMissile();
	TSubclassOf<UGameplayAbility> GetAbilityClass(ECRWeaponType Type) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> WeaponRoot;

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

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LaserBeam;

	ECRWeaponType Equipped = ECRWeaponType::None;
	bool bTriggerHeld = false;

	/** 오너 시간 배율(히트스톱)을 따르는 시계. 발사 간격 계산용. */
	float WeaponClock = 0.f;
	float NextFireTime = 0.f;

	// 칼 휘두르기
	bool bSwinging = false;
	float SwingTime = 0.f;
	float RecoverTime = 0.f;
	FCRSwingHitDelegate OnSwingHit;
	FSimpleDelegate OnSwingFinished;
	TArray<FVector> PrevBladeSamples;
	TSet<TWeakObjectPtr<AActor>> SwingHitActors;

	// 손 포즈
	FVector HandLocation = FVector::ZeroVector;
	FRotator HandRotation = FRotator::ZeroRotator;
	bool bHasHandPose = false;

	// 반동 스프링 (1 = 한 발 킥)
	float Recoil = 0.f;
	float RecoilVelocity = 0.f;
	FVector RecoilJitterOffset = FVector::ZeroVector;

	// 미사일 일제 사격 대기열 (오너 시간)
	int32 PendingMissiles = 0;
	int32 MissileSalvoIndex = 0;
	int32 MissileSalvoSize = 0;
	float NextMissileTime = 0.f;
	float MissileSpacing = 0.05f;
	TWeakObjectPtr<AActor> MissileTarget;
	FVector MissileTargetPoint = FVector::ZeroVector;

	// 손 포즈 덮어쓰기 (필살기)
	FVector OverrideHandLocation = FVector::ZeroVector;
	FRotator OverrideHandRotation = FRotator::ZeroRotator;
	float OverrideTimeLeft = 0.f;

	// 이펙트 (실시간 기준)
	float MuzzleFlashTimeLeft = 0.f;
	float LaserBeamTimeLeft = 0.f;
	FVector LaserBeamStart = FVector::ZeroVector;
	FVector LaserBeamEnd = FVector::ZeroVector;
};
