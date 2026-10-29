#include "Feedback/CRFeedbackSubsystem.h"

#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"

namespace
{
	constexpr int32 MaxDamageNumbers = 64;
	constexpr int32 ParticlesPerBatch = 512;
	constexpr float ParticleGravity = 900.f;
	constexpr float ParticleDrag = 2.5f;
	constexpr float RingDrag = 7.f;
	constexpr float RingLifetime = 0.3f;
	constexpr float VictimHalfHeight = 100.f;
	constexpr float ParticleGlowIntensity = 6.f;
	constexpr float SolidParticleIntensity = 1.1f;

	const FLinearColor SnowColor(0.9f, 0.95f, 1.f);

	// 색종이 팔레트 (참고 그림의 노랑/하늘/주황/분홍/연두)
	const FLinearColor ConfettiColors[] = {
		FLinearColor(1.f, 0.82f, 0.1f),
		FLinearColor(0.1f, 0.75f, 1.f),
		FLinearColor(1.f, 0.45f, 0.1f),
		FLinearColor(1.f, 0.3f, 0.65f),
		FLinearColor(0.45f, 0.95f, 0.3f),
	};
	constexpr float ConfettiFlutterFrequency = 9.f;

	FLinearColor GetWeaponColor(ECRWeaponType Weapon)
	{
		switch (Weapon)
		{
		case ECRWeaponType::Sword: return FLinearColor(1.f, 0.85f, 0.5f);
		case ECRWeaponType::Laser: return FLinearColor(0.2f, 0.85f, 1.f);
		case ECRWeaponType::Projectile: return FLinearColor(1.f, 0.55f, 0.15f);
		default: return FLinearColor::White;
		}
	}
}

UCRFeedbackSubsystem* UCRFeedbackSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRFeedbackSubsystem>() : nullptr;
}

bool UCRFeedbackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCRFeedbackSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCRFeedbackSubsystem, STATGROUP_Tickables);
}

void UCRFeedbackSubsystem::Deinitialize()
{
	for (const FHitStopEntry& Entry : HitStops)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Actor->CustomTimeDilation = 1.f;
		}
	}
	HitStops.Reset();
	Super::Deinitialize();
}

void UCRFeedbackSubsystem::OnHit(AActor* Attacker, ACRCharacter* Victim, ECRWeaponType Weapon, const FCRWeaponFeel& Feel,
	const FVector& HitLocation, const FVector& Direction)
{
	if (!Victim)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(Victim);

	if (Tuning.bHitFlash)
	{
		Victim->PlayHitFlash(Tuning.HitFlashDuration, Tuning.HitFlashIntensity);
	}

	if (Tuning.bDamageNumbers)
	{
		AddDamageNumber(HitLocation, Feel.Damage, Weapon);
	}

	if (Tuning.bHitStop && Feel.HitStop > 0.f)
	{
		StartHitStop(Attacker, Victim, Feel.HitStop);
	}

	if (Tuning.bKnockback && Feel.Knockback > 0.f)
	{
		Victim->ApplyKnockback(Direction * Feel.Knockback + FVector::UpVector * Feel.Knockback * Tuning.KnockbackUpRatio);
	}

	if (Tuning.bCameraShake && Feel.Shake > 0.f)
	{
		// 때린 쪽(플레이어) 카메라를 흔든다
		const ACRCharacter* Shooter = Cast<ACRCharacter>(Attacker);
		if (Shooter && Shooter->IsLocallyControlled() && Shooter->GetViewComponent())
		{
			Shooter->GetViewComponent()->AddShake(Feel.Shake);
		}
	}

	const FLinearColor WeaponColor = GetWeaponColor(Weapon);
	const float GroundZ = Victim->GetActorLocation().Z - VictimHalfHeight;
	const bool bKill = Victim->IsDead(); // 데미지가 먼저 적용되므로 이 타격으로 죽었으면 true

	if (Tuning.bHitParticles)
	{
		EmitParticles(WeaponColor, HitLocation, Direction, Tuning.HitParticleCount, 1.f, 1.f, GroundZ);
		EmitParticles(SnowColor, HitLocation, Direction, Tuning.HitParticleCount / 2, 0.6f, 1.f, GroundZ);
	}
	if (Tuning.bHitBurst)
	{
		EmitBurst(WeaponColor, HitLocation, 1.f);
	}
	if (Tuning.bShockwave)
	{
		EmitRing(WeaponColor, HitLocation, 1.f);
	}

	if (bKill && Tuning.bConfetti)
	{
		EmitConfetti(Victim->GetActorLocation() + FVector(0.f, 0.f, 40.f), Direction, GroundZ);
	}

	if (bKill && Tuning.bKillExplosion)
	{
		// 처치 확정: 눈 폭발 (몸 중심에서 사방으로)
		const FVector Center = Victim->GetActorLocation();
		const float Size = Tuning.KillExplosionSize;
		EmitParticles(SnowColor, Center, Direction, Tuning.KillExplosionCount, Tuning.KillExplosionSpeed, Size, GroundZ);
		EmitParticles(WeaponColor, Center, Direction, Tuning.KillExplosionCount / 2, Tuning.KillExplosionSpeed * 1.2f, Size * 0.8f, GroundZ);
		EmitBurst(FLinearColor::White, Center, Size * 1.5f);
		if (Tuning.bShockwave)
		{
			EmitRing(SnowColor, FVector(Center.X, Center.Y, GroundZ + 10.f), Size * 1.5f);
		}
	}
}

UCRFeedbackSubsystem::FParticle& UCRFeedbackSubsystem::NextParticle(FParticleBatch& Batch)
{
	// 고정 버퍼를 돌려 쓴다: 꽉 차면 가장 오래된 것을 덮어씀
	FParticle& P = Batch.Particles[Batch.Next];
	Batch.Next = (Batch.Next + 1) % ParticlesPerBatch;
	P = FParticle();
	P.bAlive = true;
	Batch.bAnyAlive = true;
	return P;
}

void UCRFeedbackSubsystem::EmitBurst(const FLinearColor& Color, const FVector& Location, float SizeScale)
{
	FParticleBatch* Batch = GetParticleBatch(Color);
	if (!Batch)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	FParticle& P = NextParticle(*Batch);
	P.Location = Location;
	P.Lifetime = Tuning.HitBurstDuration;
	P.Size = Tuning.HitBurstSize * SizeScale;
	P.bStatic = true;
}

void UCRFeedbackSubsystem::EmitConfetti(const FVector& Location, const FVector& Direction, float GroundZ)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const float Spin = FMath::DegreesToRadians(Tuning.ConfettiSpin);
	for (int32 i = 0; i < Tuning.ConfettiCount; ++i)
	{
		FParticleBatch* Batch = GetParticleBatch(ConfettiColors[FMath::RandHelper(UE_ARRAY_COUNT(ConfettiColors))], true);
		if (!Batch)
		{
			return;
		}
		FParticle& P = NextParticle(*Batch);
		// 위쪽 반구로 펑 터진다 (맞은 방향으로 살짝 쏠림)
		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z) + 0.6f;
		Dir = (Dir.GetSafeNormal() + Direction * 0.3f).GetSafeNormal();
		P.Location = Location;
		P.Velocity = Dir * Tuning.ConfettiSpeed * FMath::FRandRange(0.5f, 1.2f);
		P.Lifetime = Tuning.ConfettiLifetime * FMath::FRandRange(0.7f, 1.2f);
		P.Size = Tuning.ConfettiSize * FMath::FRandRange(0.7f, 1.3f);
		P.Drag = Tuning.ConfettiDrag;
		P.GroundZ = GroundZ;
		P.bConfetti = true;
		P.Rotation = FMath::VRand().ToOrientationQuat();
		P.SpinAxis = FMath::VRand();
		P.SpinSpeed = Spin * FMath::FRandRange(0.5f, 1.5f);
		P.FlutterPhase = FMath::FRandRange(0.f, UE_TWO_PI);
	}
}

void UCRFeedbackSubsystem::EmitRing(const FLinearColor& Color, const FVector& Location, float SizeScale)
{
	FParticleBatch* Batch = GetParticleBatch(Color);
	if (!Batch)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const int32 Count = FMath::Max(Tuning.ShockwaveCount, 1);
	const float StartAngle = FMath::FRandRange(0.f, UE_TWO_PI);
	for (int32 i = 0; i < Count; ++i)
	{
		const float Angle = StartAngle + UE_TWO_PI * i / Count;
		FParticle& P = NextParticle(*Batch);
		P.Location = Location;
		P.Velocity = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Tuning.ShockwaveSpeed * SizeScale;
		P.Lifetime = RingLifetime;
		P.Size = Tuning.ShockwaveSize * SizeScale;
		P.Drag = RingDrag;
		P.bGravity = false;
	}
}

void UCRFeedbackSubsystem::SpawnImpactParticles(const FVector& Location, const FVector& Normal, const FLinearColor& Color, float CountScale)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (Tuning.bHitParticles)
	{
		EmitParticles(Color, Location, Normal, FMath::RoundToInt(Tuning.HitParticleCount * CountScale), 0.7f);
	}
	if (Tuning.bHitBurst)
	{
		EmitBurst(Color, Location, 0.5f);
	}
}

void UCRFeedbackSubsystem::StartHitStop(AActor* Attacker, AActor* Victim, float Duration)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const double EndTime = FPlatformTime::Seconds() + Duration;

	if (Tuning.bHitStopGlobal)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, FMath::Max(Tuning.HitStopTimeScale, 0.0001f));
		GlobalHitStopEnd = FMath::Max(GlobalHitStopEnd, EndTime);
		return;
	}

	// 때린 쪽과 맞은 쪽만 멈춘다
	for (AActor* Actor : { Attacker, Victim })
	{
		if (!Actor)
		{
			continue;
		}
		Actor->CustomTimeDilation = Tuning.HitStopTimeScale;
		FHitStopEntry* Existing = HitStops.FindByPredicate([Actor](const FHitStopEntry& Entry) { return Entry.Actor.Get() == Actor; });
		if (Existing)
		{
			Existing->EndTime = FMath::Max(Existing->EndTime, EndTime);
		}
		else
		{
			HitStops.Add({ Actor, EndTime });
		}
	}
}

void UCRFeedbackSubsystem::UpdateHitStop()
{
	const double Now = FPlatformTime::Seconds();

	for (int32 i = HitStops.Num() - 1; i >= 0; --i)
	{
		if (Now >= HitStops[i].EndTime || !HitStops[i].Actor.IsValid())
		{
			if (AActor* Actor = HitStops[i].Actor.Get())
			{
				Actor->CustomTimeDilation = 1.f;
			}
			HitStops.RemoveAtSwap(i);
		}
	}

	if (GlobalHitStopEnd > 0.0 && Now >= GlobalHitStopEnd)
	{
		GlobalHitStopEnd = 0.0;
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	}
}

void UCRFeedbackSubsystem::AddDamageNumber(const FVector& Location, float Value, ECRWeaponType Weapon)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	FCRDamageNumber Number;
	Number.WorldLocation = Location;
	Number.Value = Value;
	Number.Duration = Tuning.DamageNumberDuration;
	Number.ScreenJitter = FVector2D(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-8.f, 8.f));
	Number.Color = GetWeaponColor(Weapon);

	if (DamageNumbers.Num() < MaxDamageNumbers)
	{
		DamageNumbers.Add(Number);
	}
	else
	{
		DamageNumbers[NextDamageNumber] = Number;
		NextDamageNumber = (NextDamageNumber + 1) % MaxDamageNumbers;
	}
}

UCRFeedbackSubsystem::FParticleBatch* UCRFeedbackSubsystem::GetParticleBatch(const FLinearColor& Color, bool bCube)
{
	for (FParticleBatch& Batch : ParticleBatches)
	{
		if (Batch.bCube == bCube && Batch.Color.Equals(Color, 0.01f))
		{
			return &Batch;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (!ParticleActor)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		ParticleActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		USceneComponent* Root = NewObject<USceneComponent>(ParticleActor, TEXT("Root"));
		ParticleActor->SetRootComponent(Root);
		Root->RegisterComponent();
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(ParticleActor);
	Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
		bCube ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetupAttachment(ParticleActor->GetRootComponent());
	Component->SetMobility(EComponentMobility::Movable);
	Component->RegisterComponent();
	// 인스턴스 메시는 Custom Primitive Data가 적용되지 않으므로 색별 MID를 쓴다
	if (UMaterialInterface* ParticleMaterial = UCRGameSettings::LoadParticleMaterial())
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ParticleMaterial, Component);
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		// 큐브(색종이, 잔해)는 빛나면 색이 하얗게 날아가므로 거의 원색으로
		Material->SetScalarParameterValue(TEXT("Intensity"), bCube ? SolidParticleIntensity : ParticleGlowIntensity);
		Component->SetMaterial(0, Material);
	}

	TArray<FTransform> Hidden;
	Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), ParticlesPerBatch);
	Component->AddInstances(Hidden, false, true);

	FParticleBatch& Batch = ParticleBatches.AddDefaulted_GetRef();
	Batch.Color = Color;
	Batch.bCube = bCube;
	Batch.ComponentIndex = ParticleComponents.Add(Component);
	Batch.Particles.SetNum(ParticlesPerBatch);
	return &Batch;
}

void UCRFeedbackSubsystem::EmitParticles(const FLinearColor& Color, const FVector& Location, const FVector& Direction, int32 Count,
	float SpeedScale, float SizeScale, float GroundZ)
{
	if (Count <= 0)
	{
		return;
	}
	FParticleBatch* Batch = GetParticleBatch(Color);
	if (!Batch)
	{
		return;
	}

	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	for (int32 i = 0; i < Count; ++i)
	{
		FParticle& P = NextParticle(*Batch);
		const FVector Dir = (Direction + FMath::VRand() * 0.9f + FVector::UpVector * 0.5f).GetSafeNormal();
		P.Location = Location;
		P.Velocity = Dir * Tuning.HitParticleSpeed * SpeedScale * FMath::FRandRange(0.5f, 1.2f);
		P.Lifetime = Tuning.HitParticleLifetime * FMath::FRandRange(0.7f, 1.3f);
		P.Size = Tuning.HitParticleSize * SizeScale * FMath::FRandRange(0.6f, 1.3f);
		P.Drag = ParticleDrag;
		P.GroundZ = GroundZ;
	}
}

void UCRFeedbackSubsystem::UpdateParticles(float DeltaTime)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	for (FParticleBatch& Batch : ParticleBatches)
	{
		if (!Batch.bAnyAlive)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Component = ParticleComponents[Batch.ComponentIndex];
		if (!Component)
		{
			continue;
		}

		bool bAnyAlive = false;
		for (int32 i = 0; i < Batch.Particles.Num(); ++i)
		{
			FParticle& P = Batch.Particles[i];
			if (!P.bAlive)
			{
				continue;
			}

			P.Age += DeltaTime;
			FTransform Transform(FQuat::Identity, P.Location, FVector::ZeroVector);
			if (P.Age >= P.Lifetime)
			{
				P.bAlive = false;
			}
			else
			{
				if (!P.bStatic)
				{
					if (P.bGravity)
					{
						P.Velocity.Z -= (P.bConfetti ? Tuning.ConfettiGravity : ParticleGravity) * DeltaTime;
					}
					P.Velocity *= FMath::Max(0.f, 1.f - P.Drag * DeltaTime);
					P.Location += P.Velocity * DeltaTime;
					if (P.bConfetti)
					{
						// 좌우로 팔랑이며 돈다
						const float Wave = FMath::Sin(P.Age * ConfettiFlutterFrequency + P.FlutterPhase);
						P.Location += FVector(Wave, FMath::Cos(P.Age * ConfettiFlutterFrequency * 0.7f + P.FlutterPhase), 0.f)
							* Tuning.ConfettiFlutter * DeltaTime;
						P.Rotation = FQuat(P.SpinAxis, P.SpinSpeed * DeltaTime) * P.Rotation;
					}

					const float Radius = P.Size * 50.f; // 구 메시 반지름 100 * 0.5
					if (Tuning.bParticleBounce && P.Location.Z - Radius < P.GroundZ && P.Velocity.Z < 0.f)
					{
						P.Location.Z = P.GroundZ + Radius;
						P.Velocity.Z = -P.Velocity.Z * Tuning.ParticleBounce;
						P.Velocity.X *= 0.7f;
						P.Velocity.Y *= 0.7f;
					}
				}

				const float Shrink = P.Size * (1.f - P.Age / P.Lifetime);
				Transform.SetLocation(P.Location);
				if (P.bConfetti)
				{
					// 납작한 종이. 수명 마지막 30%에만 작아진다
					const float Fade = FMath::Clamp((1.f - P.Age / P.Lifetime) / 0.3f, 0.f, 1.f);
					Transform.SetRotation(P.Rotation);
					Transform.SetScale3D(FVector(P.Size, P.Size * 0.6f, P.Size * 0.06f) * Fade);
				}
				else if (Tuning.bParticleStretch && !P.bStatic)
				{
					// 속도 방향으로 길쭉한 불똥
					const float Speed = P.Velocity.Size();
					Transform.SetRotation(Speed > 1.f ? P.Velocity.ToOrientationQuat() : FQuat::Identity);
					Transform.SetScale3D(FVector(Shrink * (1.f + Speed * Tuning.ParticleStretch), Shrink, Shrink));
				}
				else
				{
					Transform.SetScale3D(FVector(Shrink));
				}
				bAnyAlive = true;
			}
			Component->UpdateInstanceTransform(i, Transform, true, false, true);
		}
		Component->MarkRenderStateDirty();
		Batch.bAnyAlive = bAnyAlive;
	}
}

void UCRFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHitStop();

	const float RealDelta = FApp::GetDeltaTime();
	for (FCRDamageNumber& Number : DamageNumbers)
	{
		Number.Age += RealDelta;
	}
	UpdateParticles(DeltaTime);
}
