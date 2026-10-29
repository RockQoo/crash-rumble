#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRScreenFxSubsystem.generated.h"

class APostProcessVolume;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * 화면 단위 연출과 애니풍 렌더 설정.
 * - 포스트 프로세스: 노출 수동 고정, 비네트·모션 블러 끔, 블룸 1.0 (Unbound 볼륨)
 * - 툰 머티리얼 공용 값: 빛 방향을 레벨의 Directional Light에서 복사 (MPC)
 * - 바닥을 툰 머티리얼 + 체크무늬로 (토글)
 * - 화면 연출 머티리얼: 임팩트 프레임, 집중선, 슈퍼 프리즈 2톤. 전부 실제 시간으로 진행한다
 *   (히트스톱·시간 정지가 월드 시간을 늦추므로 월드 시간으로 재면 끝나지 않는다)
 */
UCLASS()
class CRASHRUMBLE_API UCRScreenFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRScreenFxSubsystem* Get(const UObject* WorldContext);

	/** 임팩트 프레임 2장. 쿨다운 중이면 무시 (bForce면 쿨다운 무시). */
	void RequestImpactFrame(bool bForce = false);

	/** 집중선: Amount(0~1)를 Duration초 동안. */
	void AddFocusLines(float Amount, float Duration);
	/** 매 프레임 유지하는 집중선 (달리기). 다음 프레임에 다시 부르지 않으면 꺼진다. */
	void SetSustainedFocus(float Amount) { SustainedFocus = FMath::Max(SustainedFocus, Amount); }

	/** 슈퍼 프리즈 배경 2톤 (0~1). 스텐실이 없는 픽셀만. */
	void SetFreezeAmount(float Amount) { FreezeAmount = Amount; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void ApplyTuning();
	void SetupLightDirection();
	void FindFloors();
	void ApplyFloorMaterial();

	UPROPERTY()
	TObjectPtr<APostProcessVolume> AnimeVolume;

	UPROPERTY()
	TObjectPtr<APostProcessVolume> FxVolume;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FxMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Floors;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> FloorOriginalMaterials;

	FDelegateHandle TuningChangedHandle;
	bool bFloorToon = false;

	double Now = 0.0;
	double ImpactStart = -1.0;
	double ImpactCooldownEnd = 0.0;
	float FocusTimed = 0.f;
	double FocusEnd = 0.0;
	float SustainedFocus = 0.f;
	float FreezeAmount = 0.f;
	float SeedTimer = 0.f;
	float Seed = 0.f;
};
