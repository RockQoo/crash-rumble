#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weapon/CRWeaponTypes.h"
#include "CRFeedbackSubsystem.generated.h"

class ACRCharacter;
class UInstancedStaticMeshComponent;

/** 화면에 떠오르는 데미지 숫자 하나 (HUD가 그린다). */
struct FCRDamageNumber
{
	FVector WorldLocation = FVector::ZeroVector;
	float Value = 0.f;
	float Age = 0.f;
	float Duration = 1.f;
	FVector2D ScreenJitter = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

/**
 * 타격감 처리. 무기 명중 시 CRCombat이 OnHit를 호출하면 토글된 항목만 실행한다.
 * 번쩍임(캐릭터), 데미지 숫자, 히트스톱, 넉백, 카메라 흔들림, 피격 파티클.
 * 데미지 숫자와 파티클은 고정 크기 버퍼를 재사용한다 (풀링).
 */
UCLASS()
class CRASHRUMBLE_API UCRFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRFeedbackSubsystem* Get(const UObject* WorldContext);

	void OnHit(AActor* Attacker, ACRCharacter* Victim, ECRWeaponType Weapon, const FCRWeaponFeel& Feel,
		const FVector& HitLocation, const FVector& Direction);

	void SpawnImpactParticles(const FVector& Location, const FVector& Normal, const FLinearColor& Color, float CountScale = 1.f);

	const TArray<FCRDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	struct FHitStopEntry
	{
		TWeakObjectPtr<AActor> Actor;
		double EndTime = 0.0;
	};

	struct FParticle
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Lifetime = 0.f;
		float Size = 0.f;
		float GroundZ = -UE_BIG_NUMBER; // 이 높이에서 튕긴다
		float Drag = 0.f;
		bool bStatic = false; // 섬광: 제자리에서 줄어들기만 함
		bool bGravity = true;
		bool bAlive = false;

		// 색종이: 납작한 큐브가 돌며 팔랑팔랑 떨어진다
		bool bConfetti = false;
		FQuat Rotation = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinSpeed = 0.f;
		float FlutterPhase = 0.f;
	};

	/** 색 + 모양 하나당 인스턴스 메시 하나. 인스턴스는 미리 만들어 두고 재사용. */
	struct FParticleBatch
	{
		FLinearColor Color;
		bool bCube = false;
		int32 ComponentIndex = INDEX_NONE;
		TArray<FParticle> Particles;
		int32 Next = 0;
		bool bDirty = false;
		bool bAnyAlive = false;
	};

	void StartHitStop(AActor* Attacker, AActor* Victim, float Duration);
	void UpdateHitStop();
	void AddDamageNumber(const FVector& Location, float Value, ECRWeaponType Weapon);
	FParticleBatch* GetParticleBatch(const FLinearColor& Color, bool bCube = false);
	/** 처치 시 색종이. */
	void EmitConfetti(const FVector& Location, const FVector& Direction, float GroundZ);
	FParticle& NextParticle(FParticleBatch& Batch);
	void EmitParticles(const FLinearColor& Color, const FVector& Location, const FVector& Direction, int32 Count, float SpeedScale,
		float SizeScale = 1.f, float GroundZ = -UE_BIG_NUMBER);
	void EmitBurst(const FLinearColor& Color, const FVector& Location, float SizeScale);
	/** 수평으로 퍼지는 충격파 링 (조각들이 원형으로 바깥으로 밀려남). */
	void EmitRing(const FLinearColor& Color, const FVector& Location, float SizeScale);
	void UpdateParticles(float DeltaTime);

	TArray<FHitStopEntry> HitStops;
	double GlobalHitStopEnd = 0.0;

	TArray<FCRDamageNumber> DamageNumbers;
	int32 NextDamageNumber = 0;

	UPROPERTY()
	TObjectPtr<AActor> ParticleActor;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ParticleComponents;

	TArray<FParticleBatch> ParticleBatches;
};
