#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/SBWeaponTypes.h"
#include "SBWeaponComponent.generated.h"

class UGameplayAbility;
class UPointLightComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_OneParam(FSBSwingHitDelegate, const FHitResult&);

USTRUCT()
struct FSBWeaponVisual
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
 * 데미지 적용은 GAS 어빌리티(SBWeaponAbilities)가 한다.
 */
UCLASS(ClassGroup = (SnowBattle), meta = (BlueprintSpawnableComponent))
class SNOWBATTLE_API USBWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USBWeaponComponent();

	/** 캐릭터가 파츠를 조립한 뒤 호출. Hand = 무기를 붙일 손. */
	void Initialize(USceneComponent* InHand);

	void Equip(ESBWeaponType Type);
	ESBWeaponType GetEquipped() const { return Equipped; }

	void SetTriggerHeld(bool bHeld) { bTriggerHeld = bHeld; }

	// 어빌리티에서 호출
	void StartSwing(FSBSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished);
	/** 맞힌 눈사람 목록을 채우고 개수를 돌려준다 (관통 수 반영). */
	int32 FireLaser(TArray<FHitResult>& OutTargetHits);
	void FireProjectile();

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
	FVector GetMuzzleLocation() const;
	FVector GetFireDirection(const FVector& Start) const;
	TSubclassOf<UGameplayAbility> GetAbilityClass(ESBWeaponType Type) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> WeaponRoot;

	UPROPERTY()
	TMap<ESBWeaponType, FSBWeaponVisual> Visuals;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SwordBlade;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LaserBeam;

	ESBWeaponType Equipped = ESBWeaponType::None;
	bool bTriggerHeld = false;

	/** 오너 시간 배율(히트스톱)을 따르는 시계. 발사 간격 계산용. */
	float WeaponClock = 0.f;
	float NextFireTime = 0.f;

	// 칼 휘두르기
	bool bSwinging = false;
	float SwingTime = 0.f;
	float RecoverTime = 0.f;
	FSBSwingHitDelegate OnSwingHit;
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

	// 이펙트 (실시간 기준)
	float MuzzleFlashTimeLeft = 0.f;
	float LaserBeamTimeLeft = 0.f;
	FVector LaserBeamStart = FVector::ZeroVector;
	FVector LaserBeamEnd = FVector::ZeroVector;
};
