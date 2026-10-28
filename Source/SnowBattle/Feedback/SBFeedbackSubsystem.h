#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Weapon/SBWeaponTypes.h"
#include "SBFeedbackSubsystem.generated.h"

class ASBSnowmanCharacter;
class UInstancedStaticMeshComponent;

/** 화면에 떠오르는 데미지 숫자 하나 (HUD가 그린다). */
struct FSBDamageNumber
{
	FVector WorldLocation = FVector::ZeroVector;
	float Value = 0.f;
	float Age = 0.f;
	float Duration = 1.f;
	FVector2D ScreenJitter = FVector2D::ZeroVector;
	FLinearColor Color = FLinearColor::White;
};

/**
 * 타격감 처리. 무기 명중 시 SBCombat이 OnHit를 호출하면 토글된 항목만 실행한다.
 * 번쩍임(캐릭터), 데미지 숫자, 히트스톱, 넉백, 카메라 흔들림, 피격 파티클.
 * 데미지 숫자와 파티클은 고정 크기 버퍼를 재사용한다 (풀링).
 */
UCLASS()
class SNOWBATTLE_API USBFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static USBFeedbackSubsystem* Get(const UObject* WorldContext);

	void OnHit(AActor* Attacker, ASBSnowmanCharacter* Victim, ESBWeaponType Weapon, const FSBWeaponFeel& Feel,
		const FVector& HitLocation, const FVector& Direction);

	void SpawnImpactParticles(const FVector& Location, const FVector& Normal, const FLinearColor& Color, float CountScale = 1.f);

	const TArray<FSBDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }

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
		bool bStatic = false; // 섬광: 제자리에서 줄어들기만 함
		bool bAlive = false;
	};

	/** 색 하나당 인스턴스 메시 하나. 인스턴스는 미리 만들어 두고 재사용. */
	struct FParticleBatch
	{
		FLinearColor Color;
		int32 ComponentIndex = INDEX_NONE;
		TArray<FParticle> Particles;
		int32 Next = 0;
		bool bDirty = false;
		bool bAnyAlive = false;
	};

	void StartHitStop(AActor* Attacker, AActor* Victim, float Duration);
	void UpdateHitStop();
	void AddDamageNumber(const FVector& Location, float Value, ESBWeaponType Weapon);
	FParticleBatch* GetParticleBatch(const FLinearColor& Color);
	void EmitParticles(const FLinearColor& Color, const FVector& Location, const FVector& Direction, int32 Count, float SpeedScale);
	void EmitBurst(const FLinearColor& Color, const FVector& Location, float SizeScale);
	void UpdateParticles(float DeltaTime);

	TArray<FHitStopEntry> HitStops;
	double GlobalHitStopEnd = 0.0;

	TArray<FSBDamageNumber> DamageNumbers;
	int32 NextDamageNumber = 0;

	UPROPERTY()
	TObjectPtr<AActor> ParticleActor;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ParticleComponents;

	TArray<FParticleBatch> ParticleBatches;
};
