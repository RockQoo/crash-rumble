#include "Weapon/CRWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRActorPool.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/CRFeedbackSubsystem.h"
#include "Feedback/CRFxSubsystem.h"
#include "GAS/CRCombat.h"
#include "GAS/CRWeaponAbilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Core/CRTargetSpawner.h"
#include "Engine/OverlapResult.h"
#include "Map/CRBlockArena.h"
#include "Weapon/CRMissile.h"
#include "Weapon/CRProjectile.h"

namespace
{
	const TCHAR* CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylinderMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	// 칼: 손잡이 끝에서 칼날이 시작하는 위치
	constexpr float BladeStart = 12.f;
	constexpr int32 BladeSampleCount = 4;

	// 칼 자세 (캡슐 기준): 몸 앞 반지름 Radius의 호 위를 손이 돈다
	constexpr float SwingRadius = 65.f;
	constexpr float SwingHeight = -25.f;
	constexpr float IdleYaw = 50.f;
	constexpr float IdlePitch = 50.f;
	constexpr float SwingRecoverDuration = 0.2f;

	// 총 자세
	const FVector GunHandLocation(40.f, 25.f, -10.f);
	constexpr float HandPoseInterpSpeed = 18.f;

	const FLinearColor LaserColor(0.2f, 0.85f, 1.f);
	const FLinearColor ProjectileFlashColor(1.f, 0.55f, 0.15f);
	const FLinearColor LightningGunColor(0.4f, 0.8f, 1.6f);
	const FLinearColor MissileFlashColor(1.f, 0.7f, 0.3f);
	const FLinearColor FlameGunColor(1.f, 0.3f, 0.8f);

	// 미사일 표적: 커서 근처(300cm)에 적이 있으면 그 적.
	// 없으면 앞쪽 일정 거리 안의 적들에게 나눠 보내고, 적도 없으면 사거리 안 바닥
	constexpr float MissileLockRadius = 300.f;
	constexpr float MissileSeekRange = 2500.f;
	constexpr float MissileSeekCone = 0.3f;
	constexpr float MissileMaxGroundRange = 3000.f;
	constexpr float SuperMissileSpacingScale = 0.5f;
	constexpr float GlowIntensity = 12.f;

	FVector SwordHandLocation(float Yaw)
	{
		const float Rad = FMath::DegreesToRadians(Yaw);
		return FVector(FMath::Cos(Rad) * SwingRadius, FMath::Sin(Rad) * SwingRadius, SwingHeight);
	}
}

UCRWeaponComponent::UCRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCRWeaponComponent::Initialize(USceneComponent* InHand)
{
	if (!InHand)
	{
		return;
	}

	AActor* Owner = GetOwner();
	WeaponRoot = NewObject<USceneComponent>(Owner, TEXT("WeaponRoot"));
	WeaponRoot->SetupAttachment(InHand);
	WeaponRoot->SetUsingAbsoluteScale(true); // 손 구의 축소 스케일을 무기에 물려주지 않는다
	WeaponRoot->RegisterComponent();

	BuildVisuals();
	Equip(ECRWeaponType::None);
}

void UCRWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnSwingHit.Unbind();
	OnSwingFinished.Unbind();
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* UCRWeaponComponent::MakePart(USceneComponent* Parent, const TCHAR* MeshPath, const FVector& Location,
	const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color, bool bGlow, float Intensity)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	const bool bToon = UCRTuningSubsystem::GetSettings(this).bToonShading;
	Part->SetCastShadow(!bGlow && !bToon);
	Part->SetupAttachment(Parent ? Parent : GetOwner()->GetRootComponent());
	Part->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Part->RegisterComponent();

	UMaterialInterface* Material = bGlow ? UCRGameSettings::LoadGlowMaterial() : UCRGameSettings::LoadSurfaceMaterial(this);
	if (Material)
	{
		Part->SetMaterial(0, Material);
		// 알파 = 툰 Flat 플래그 (0 = 명암 있음)
		Part->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, bGlow ? Color.A : 0.f));
		Part->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, bGlow ? Intensity : 0.f);
	}
	if (!bGlow)
	{
		SurfaceParts.Add(Part);
		SurfaceMaterial = Material;
	}
	return Part;
}

void UCRWeaponComponent::BuildVisuals()
{
	const FRotator AlongX(-90.f, 0.f, 0.f); // 원기둥(Z축)을 X축 방향으로 눕힘

	// 칼
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Sword);
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(0, 0, 0), FVector(0.05f, 0.05f, 0.2f), AlongX, FLinearColor(0.25f, 0.12f, 0.05f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(10, 0, 0), FVector(0.05f, 0.3f, 0.05f), FRotator::ZeroRotator, FLinearColor(1.f, 0.75f, 0.2f), false));
		SwordBlade = MakePart(WeaponRoot, CubeMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, FLinearColor(0.85f, 0.9f, 1.f), false);
		Visual.Parts.Add(SwordBlade);
		UpdateSwordBlade();
	}

	// 레이저총
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Laser);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.32f, 0.12f, 0.16f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.15f, 0.18f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 6), FVector(0.07f, 0.07f, 0.28f), AlongX, FLinearColor(0.3f, 0.3f, 0.35f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(50, 0, 6), FVector(0.08f), FRotator::ZeroRotator, LaserColor, true, 4.f));
		Visual.MuzzleOffset = FVector(54, 0, 6);
		Visual.FlashColor = LaserColor;
	}

	// 투사체총
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Projectile);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.36f, 0.16f, 0.2f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.35f, 0.15f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 8), FVector(0.13f, 0.13f, 0.24f), AlongX, FLinearColor(0.2f, 0.2f, 0.2f), false));
		Visual.MuzzleOffset = FVector(50, 0, 8);
		Visual.FlashColor = ProjectileFlashColor;
	}

	// 번개총: 몸통 + 코일 링 + 발광 끝
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Lightning);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.3f, 0.13f, 0.16f), FRotator::ZeroRotator, FLinearColor(0.2f, 0.22f, 0.3f), false));
		for (int32 i = 0; i < 3; ++i)
		{
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(30 + i * 7, 0, 6), FVector(0.12f, 0.12f, 0.03f), AlongX, FLinearColor(0.9f, 0.75f, 0.2f), false));
		}
		Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(52, 0, 6), FVector(0.1f), FRotator::ZeroRotator, LightningGunColor, true, 4.f));
		Visual.MuzzleOffset = FVector(56, 0, 6);
		Visual.FlashColor = LightningGunColor;
	}

	// 미사일 발사대: 몸통 + 발사관 3개
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Missile);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(10, 0, 6), FVector(0.3f, 0.2f, 0.2f), FRotator::ZeroRotator, FLinearColor(0.3f, 0.35f, 0.2f), false));
		for (int32 i = -1; i <= 1; ++i)
		{
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(30, i * 7.f, 12), FVector(0.06f, 0.06f, 0.3f), AlongX, FLinearColor(0.75f, 0.75f, 0.8f), false));
		}
		Visual.MuzzleOffset = FVector(46, 0, 12);
		Visual.FlashColor = MissileFlashColor;
	}

	// 불꽃 발사기: 통 + 분홍 노즐
	{
		FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Flame);
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(8, 0, 6), FVector(0.14f, 0.14f, 0.3f), AlongX, FLinearColor(0.6f, 0.15f, 0.4f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(32, 0, 6), FVector(0.06f, 0.06f, 0.22f), AlongX, FLinearColor(0.25f, 0.25f, 0.3f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(44, 0, 6), FVector(0.08f), FRotator::ZeroRotator, FlameGunColor, true, 4.f));
		Visual.MuzzleOffset = FVector(48, 0, 6);
		Visual.FlashColor = FlameGunColor;
	}

	MuzzleFlash = MakePart(WeaponRoot, SphereMesh, FVector::ZeroVector, FVector(0.3f, 0.18f, 0.18f), FRotator::ZeroRotator, FLinearColor::White, true, GlowIntensity);
	MuzzleFlash->SetVisibility(false);

	MuzzleLight = NewObject<UPointLightComponent>(GetOwner());
	MuzzleLight->SetupAttachment(WeaponRoot);
	MuzzleLight->SetAttenuationRadius(400.f);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->RegisterComponent();

	// 빔은 월드 좌표로 직접 배치
	LaserBeam = MakePart(nullptr, CylinderMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, LaserColor, true, GlowIntensity);
	LaserBeam->SetUsingAbsoluteLocation(true);
	LaserBeam->SetUsingAbsoluteRotation(true);
	LaserBeam->SetUsingAbsoluteScale(true);
	LaserBeam->SetVisibility(false);
}

void UCRWeaponComponent::Equip(ECRWeaponType Type)
{
	const bool bChanged = Equipped != Type;
	Equipped = Type;
	for (TPair<ECRWeaponType, FCRWeaponVisual>& Pair : Visuals)
	{
		for (UStaticMeshComponent* Part : Pair.Value.Parts)
		{
			Part->SetVisibility(Pair.Key == Type);
		}
	}

	if (bSwinging)
	{
		bSwinging = false;
		OnSwingFinished.ExecuteIfBound();
	}
	RecoverTime = 0.f;
	Recoil = 0.f;
	RecoilVelocity = 0.f;
	NextFireTime = FMath::Min(NextFireTime, WeaponClock + 0.1f);

	// 무기 전환 신호: 총구(칼은 칼끝)에 십자 반짝임 (플레이어만)
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (bChanged && Type != ECRWeaponType::None && Pawn && Pawn->IsPlayerControlled()
		&& UCRTuningSubsystem::GetSettings(this).bSparkle && WeaponRoot)
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			const FVector Tip = Type == ECRWeaponType::Sword
				? WeaponRoot->GetComponentTransform().TransformPosition(FVector(BladeStart + UCRTuningSubsystem::GetSettings(this).SwordLength, 0.f, 0.f))
				: GetMuzzleLocation();
			Fx->SpawnSparkle(Tip, UCRTuningSubsystem::GetSettings(this).SparkleSize, WeaponRoot);
		}
	}
}

void UCRWeaponComponent::RefreshSurfaceMaterial()
{
	UMaterialInterface* Material = UCRGameSettings::LoadSurfaceMaterial(this);
	if (!Material || Material == SurfaceMaterial)
	{
		return;
	}
	SurfaceMaterial = Material;
	const bool bToon = UCRTuningSubsystem::GetSettings(this).bToonShading;
	for (UStaticMeshComponent* Part : SurfaceParts)
	{
		if (Part)
		{
			Part->SetMaterial(0, Material);
			Part->SetCastShadow(!bToon);
		}
	}
}

TSubclassOf<UGameplayAbility> UCRWeaponComponent::GetAbilityClass(ECRWeaponType Type) const
{
	switch (Type)
	{
	case ECRWeaponType::Sword: return UCRGA_SwordSwing::StaticClass();
	case ECRWeaponType::Laser: return UCRGA_LaserFire::StaticClass();
	case ECRWeaponType::Projectile: return UCRGA_ProjectileFire::StaticClass();
	case ECRWeaponType::Lightning: return UCRGA_LightningFire::StaticClass();
	case ECRWeaponType::Missile: return UCRGA_MissileFire::StaticClass();
	case ECRWeaponType::Flame: return UCRGA_FlameFire::StaticClass();
	default: return nullptr;
	}
}

void UCRWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!WeaponRoot)
	{
		return;
	}

	WeaponClock += DeltaTime;

	if (Equipped == ECRWeaponType::Sword)
	{
		UpdateSwordBlade();
	}
	TryFire();
	UpdateMissileQueue();
	UpdateSwing(DeltaTime);
	OverrideTimeLeft = FMath::Max(0.f, OverrideTimeLeft - DeltaTime);
	UpdateRecoil(DeltaTime);
	UpdateHandPose(DeltaTime);
	UpdateEffects(FApp::GetDeltaTime());
}

void UCRWeaponComponent::TryFire()
{
	if (!bTriggerHeld || Equipped == ECRWeaponType::None || bSwinging || WeaponClock < NextFireTime)
	{
		return;
	}

	ACRCharacter* Snowman = Cast<ACRCharacter>(GetOwner());
	UAbilitySystemComponent* ASC = Snowman ? Snowman->GetAbilitySystemComponent() : nullptr;
	if (!ASC || Snowman->IsDead())
	{
		return;
	}

	if (ASC->TryActivateAbilityByClass(GetAbilityClass(Equipped)))
	{
		const FCRWeaponFeel Feel = CRWeapon::GetFeel(UCRTuningSubsystem::GetSettings(this), Equipped);
		NextFireTime = WeaponClock + Feel.Interval;
	}
}

void UCRWeaponComponent::StartSwing(FCRSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished)
{
	OnSwingHit = InOnHit;
	OnSwingFinished = InOnFinished;
	bSwinging = true;
	SwingTime = 0.f;
	RecoverTime = 0.f;
	SwingHitActors.Reset();
	PrevBladeSamples.Reset();
}

void UCRWeaponComponent::UpdateSwing(float DeltaTime)
{
	if (RecoverTime > 0.f)
	{
		RecoverTime = FMath::Max(0.f, RecoverTime - DeltaTime);
	}
	if (!bSwinging)
	{
		return;
	}

	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const bool bFirst = SwingTime == 0.f;
	SwingTime += DeltaTime;

	const float Alpha = FMath::Clamp(SwingTime / FMath::Max(Tuning.SwordSwingDuration, 0.01f), 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f); // 빠르게 나가서 끝에서 감속
	const float HalfArc = Tuning.SwordArcAngle * 0.5f;
	const float Yaw = FMath::Lerp(HalfArc, -HalfArc, Eased);

	HandLocation = SwordHandLocation(Yaw);
	HandRotation = FRotator(FMath::Lerp(10.f, -5.f, Eased), Yaw, 0.f);
	bHasHandPose = true;

	// 손 위치를 이번 프레임 값으로 반영한 뒤 칼날 스윕
	if (USceneComponent* Hand = WeaponRoot->GetAttachParent())
	{
		Hand->SetRelativeLocationAndRotation(HandLocation, HandRotation);
	}
	SweepBlade(bFirst);

	if (Alpha >= 1.f)
	{
		bSwinging = false;
		RecoverTime = SwingRecoverDuration;
		FSimpleDelegate Finished = OnSwingFinished;
		OnSwingHit.Unbind();
		OnSwingFinished.Unbind();
		Finished.ExecuteIfBound();
	}
}

void UCRWeaponComponent::SweepBlade(bool bFirstSample)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FTransform RootTransform = WeaponRoot->GetComponentTransform();

	TArray<FVector> Samples;
	Samples.Reserve(BladeSampleCount);
	for (int32 i = 0; i < BladeSampleCount; ++i)
	{
		const float T = (i + 1.f) / BladeSampleCount;
		Samples.Add(RootTransform.TransformPosition(FVector(BladeStart + Tuning.SwordLength * T, 0.f, 0.f)));
	}
	if (bFirstSample || PrevBladeSamples.Num() != Samples.Num())
	{
		PrevBladeSamples = Samples;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRSwordSweep), false, GetOwner());
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Tuning.SwordHitRadius);

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, PrevBladeSamples[i], Samples[i], FQuat::Identity, ObjectParams, Shape, Params);
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && !SwingHitActors.Contains(HitActor))
			{
				SwingHitActors.Add(HitActor);
				OnSwingHit.ExecuteIfBound(Hit);
			}
		}
	}
	PrevBladeSamples = Samples;
}

void UCRWeaponComponent::UpdateSwordBlade()
{
	if (SwordBlade)
	{
		const float Length = UCRTuningSubsystem::GetSettings(this).SwordLength;
		SwordBlade->SetRelativeLocation(FVector(BladeStart + Length * 0.5f, 0.f, 0.f));
		SwordBlade->SetRelativeScale3D(FVector(Length / 100.f, 0.08f, 0.02f));
	}
}

void UCRWeaponComponent::UpdateHandPose(float DeltaTime)
{
	if (bSwinging)
	{
		return; // UpdateSwing에서 이미 설정
	}

	FVector TargetLocation;
	FRotator TargetRotation;

	if (OverrideTimeLeft > 0.f)
	{
		TargetLocation = OverrideHandLocation;
		TargetRotation = OverrideHandRotation;
	}
	else if (Equipped == ECRWeaponType::Sword)
	{
		TargetLocation = SwordHandLocation(IdleYaw);
		TargetRotation = FRotator(IdlePitch, IdleYaw, 0.f);
	}
	else if (Equipped != ECRWeaponType::None)
	{
		// 조준점을 향하도록 손을 돌린다
		const AActor* Owner = GetOwner();
		const ACRCharacter* Snowman = Cast<ACRCharacter>(Owner);
		const FTransform ActorTransform = Owner->GetActorTransform();
		FRotator Aim = FRotator::ZeroRotator;
		FVector AimPoint;
		if (Snowman && Snowman->GetWeaponAimPoint(AimPoint))
		{
			const FVector HandWorld = ActorTransform.TransformPosition(GunHandLocation);
			const FVector LocalDir = ActorTransform.InverseTransformVectorNoScale(AimPoint - HandWorld);
			Aim = LocalDir.Rotation();
			Aim.Pitch = FMath::Clamp(Aim.Pitch, -60.f, 60.f);
			Aim.Yaw = FMath::Clamp(FRotator::NormalizeAxis(Aim.Yaw), -50.f, 50.f);
		}
		TargetLocation = GunHandLocation;
		TargetRotation = Aim;
	}
	else
	{
		bHasHandPose = false;
		return;
	}

	if (!bHasHandPose || RecoverTime <= 0.f)
	{
		// 평소: 부드럽게 따라감
		HandLocation = bHasHandPose ? FMath::VInterpTo(HandLocation, TargetLocation, DeltaTime, HandPoseInterpSpeed) : TargetLocation;
		HandRotation = bHasHandPose ? FMath::RInterpTo(HandRotation, TargetRotation, DeltaTime, HandPoseInterpSpeed) : TargetRotation;
	}
	else
	{
		// 휘두른 직후: 끝 자세에서 기본 자세로 복귀
		const float Speed = 1.f / SwingRecoverDuration;
		HandLocation = FMath::VInterpTo(HandLocation, TargetLocation, DeltaTime, Speed * 4.f);
		HandRotation = FMath::RInterpTo(HandRotation, TargetRotation, DeltaTime, Speed * 4.f);
	}
	bHasHandPose = true;
}

void UCRWeaponComponent::UpdateRecoil(float DeltaTime)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 스프링-댐퍼로 0에 복귀
	const float Accel = -Tuning.RecoilStiffness * Recoil - Tuning.RecoilDamping * RecoilVelocity;
	RecoilVelocity += Accel * DeltaTime;
	Recoil += RecoilVelocity * DeltaTime;
	if (FMath::Abs(Recoil) < 0.001f && FMath::Abs(RecoilVelocity) < 0.01f)
	{
		Recoil = 0.f;
		RecoilVelocity = 0.f;
	}

	const float Shake = FMath::Clamp(FMath::Abs(Recoil), 0.f, 1.f);
	RecoilJitterOffset = FMath::VRand() * Tuning.RecoilJitter * Shake;
}

bool UCRWeaponComponent::GetHandPose(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!bHasHandPose)
	{
		return false;
	}

	OutLocation = HandLocation;
	OutRotation = HandRotation;

	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (Tuning.bWeaponRecoil && Recoil != 0.f)
	{
		OutLocation -= HandRotation.Vector() * Recoil * Tuning.RecoilKickback;
		OutLocation += RecoilJitterOffset;
		OutRotation.Pitch += Recoil * Tuning.RecoilPitch;
	}
	return true;
}

FVector UCRWeaponComponent::GetMuzzleLocation() const
{
	const FCRWeaponVisual* Visual = Visuals.Find(Equipped);
	return WeaponRoot->GetComponentTransform().TransformPosition(Visual ? Visual->MuzzleOffset : FVector::ZeroVector);
}

FVector UCRWeaponComponent::GetFireDirection(const FVector& Start) const
{
	const ACRCharacter* Snowman = Cast<ACRCharacter>(GetOwner());
	FVector AimPoint;
	if (Snowman && Snowman->GetWeaponAimPoint(AimPoint))
	{
		const FVector ToAim = AimPoint - Start;
		// 조준점이 총구 뒤/너무 가까우면 몸 정면으로
		if (ToAim.SizeSquared() > FMath::Square(50.f) && FVector::DotProduct(ToAim, GetOwner()->GetActorForwardVector()) > 0.f)
		{
			return ToAim.GetSafeNormal();
		}
	}
	return WeaponRoot->GetForwardVector();
}

int32 UCRWeaponComponent::FireLaser(TArray<FHitResult>& OutTargetHits)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FVector Start = GetMuzzleLocation();
	const FVector End = Start + GetFireDirection(Start) * Tuning.LaserRange;

	// 눈사람을 맞히면 그 대상을 빼고 다시 쏜다 (관통 수만큼)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRLaser), false, GetOwner());
	FVector BeamEnd = End;
	const int32 MaxTargets = FMath::Max(Tuning.LaserPierce, 0) + 1;
	const ACRCharacter* Shooter = Cast<ACRCharacter>(GetOwner());
	const ECRTeam ShooterTeam = Shooter ? Shooter->GetTeam() : ECRTeam::None;
	constexpr int32 MaxPassThrough = 16; // 아군/시체를 통과하는 최대 횟수
	for (int32 Iteration = 0; Iteration < MaxTargets + MaxPassThrough; ++Iteration)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, CRCombat::GetWeaponQueryParams(), Params))
		{
			break;
		}
		if (!Cast<ACRCharacter>(Hit.GetActor()))
		{
			// 벽/바닥: 빔이 여기서 끝나고 작은 불꽃 (블럭이면 HP 1)
			BeamEnd = Hit.ImpactPoint;
			CRCombat::DamageWorld(Hit, 1, GetOwner());
			if (UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(this))
			{
				Feedback->SpawnImpactParticles(Hit.ImpactPoint, Hit.ImpactNormal, LaserColor, 0.5f);
			}
			break;
		}
		// 쓰러진 눈사람과 (팀 전투) 아군은 빔이 통과
		const ACRCharacter* HitSnowman = Cast<ACRCharacter>(Hit.GetActor());
		if (HitSnowman->IsDead() || (ShooterTeam != ECRTeam::None && HitSnowman->GetTeam() == ShooterTeam))
		{
			Params.AddIgnoredActor(Hit.GetActor());
			continue;
		}
		OutTargetHits.Add(Hit);
		if (OutTargetHits.Num() >= MaxTargets)
		{
			BeamEnd = Hit.ImpactPoint;
			break;
		}
		Params.AddIgnoredActor(Hit.GetActor());
	}

	LaserBeamStart = Start;
	LaserBeamEnd = BeamEnd;
	LaserBeamTimeLeft = Tuning.LaserBeamDuration;

	PlayFireFeedback();
	return OutTargetHits.Num();
}

void UCRWeaponComponent::FireProjectile()
{
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	if (!Pool)
	{
		return;
	}
	const FVector Start = GetMuzzleLocation();
	const FVector Direction = GetFireDirection(Start);
	if (ACRProjectile* Projectile = Pool->Acquire<ACRProjectile>(FTransform(Direction.Rotation(), Start)))
	{
		Projectile->Launch(GetOwner(), Direction);
	}
	PlayFireFeedback();
}

void UCRWeaponComponent::SetHandOverride(const FVector& Location, const FRotator& Rotation, float Duration)
{
	OverrideHandLocation = Location;
	OverrideHandRotation = Rotation;
	OverrideTimeLeft = Duration;
}

void UCRWeaponComponent::FireLightning(TArray<FHitResult>& OutTargetHits)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FVector Start = GetMuzzleLocation();
	const FVector End = Start + GetFireDirection(Start) * Tuning.LightningRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRLightning), false, GetOwner());
	const ACRCharacter* Shooter = Cast<ACRCharacter>(GetOwner());
	const ECRTeam ShooterTeam = Shooter ? Shooter->GetTeam() : ECRTeam::None;
	FVector BoltEnd = End;
	for (int32 Iteration = 0; Iteration < 16; ++Iteration)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, CRCombat::GetWeaponQueryParams(), Params))
		{
			break;
		}
		const ACRCharacter* HitSnowman = Cast<ACRCharacter>(Hit.GetActor());
		if (HitSnowman && (HitSnowman->IsDead() || (ShooterTeam != ECRTeam::None && HitSnowman->GetTeam() == ShooterTeam)))
		{
			Params.AddIgnoredActor(Hit.GetActor());
			continue;
		}
		// 끝점은 대상 중심이 아니라 표면 (중심이면 끝 섬광이 몸 안에 묻혀 목에 링처럼 보인다)
		BoltEnd = Hit.ImpactPoint;
		if (HitSnowman)
		{
			OutTargetHits.Add(Hit);
		}
		else if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			CRCombat::DamageWorld(Hit, Tuning.LightningBlockDamage, GetOwner());
			// 바닥에 맞으면 큐브 잔해 8개
			if (Tuning.bCubeDebris)
			{
				Fx->SpawnDebris(Hit.ImpactPoint, Hit.ImpactNormal, 8, FLinearColor(0.55f, 0.55f, 0.6f), FLinearColor(0.35f, 0.35f, 0.4f), 1.f, Hit.ImpactPoint.Z - 1.f);
			}
		}
		break;
	}
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		Fx->SpawnLightning(Start, BoltEnd, Tuning.LightningBoltDuration);
	}
	PlayFireFeedback();
}

void UCRWeaponComponent::FireMissiles(int32 Count, bool bSuper)
{
	const ACRCharacter* Snowman = Cast<ACRCharacter>(GetOwner());
	FVector AimPoint = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 1000.f;
	if (Snowman)
	{
		Snowman->GetWeaponAimPoint(AimPoint);
	}
	const AActor* Owner = GetOwner();
	const FVector From = Owner->GetActorLocation();
	const ECRTeam OwnerTeam = Snowman ? Snowman->GetTeam() : ECRTeam::None;
	auto IsEnemy = [Snowman, OwnerTeam](const ACRCharacter* Candidate)
	{
		return Candidate && Candidate != Snowman && !Candidate->IsDead() && !Candidate->IsInPool() && !Candidate->IsHidden()
			&& (OwnerTeam == ECRTeam::None || (Candidate->GetTeam() != ECRTeam::None && Candidate->GetTeam() != OwnerTeam));
	};

	MissileTargets.Reset();
	MissileTargetPoint = AimPoint;

	// 1) 커서 근처(300cm)의 적 하나 (맵에 놓인 허수아비 포함)
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	GetWorld()->OverlapMultiByObjectType(Overlaps, AimPoint, FQuat::Identity, Objects, FCollisionShape::MakeSphere(MissileLockRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CRMissileLock), false, Owner));
	float Best = UE_BIG_NUMBER;
	ACRCharacter* Locked = nullptr;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACRCharacter* Candidate = Cast<ACRCharacter>(Overlap.GetActor());
		const float Distance = Candidate ? FVector::DistSquared(Candidate->GetActorLocation(), AimPoint) : UE_BIG_NUMBER;
		if (IsEnemy(Candidate) && Distance < Best)
		{
			Best = Distance;
			Locked = Candidate;
		}
	}
	if (Locked)
	{
		MissileTargets.Add(Locked);
	}
	else
	{
		// 2) 숄더뷰로 지평선을 조준하면 커서 아래가 너무 멀다: 앞쪽의 가까운 적들에게 나눠 보낸다
		TArray<FOverlapResult> Nearby;
		GetWorld()->OverlapMultiByObjectType(Nearby, From, FQuat::Identity, Objects, FCollisionShape::MakeSphere(MissileSeekRange),
			FCollisionQueryParams(SCENE_QUERY_STAT(CRMissileSeek), false, Owner));
		TArray<TPair<float, ACRCharacter*>> Candidates;
		const FVector Forward = (AimPoint - From).GetSafeNormal2D();
		for (const FOverlapResult& Overlap : Nearby)
		{
			ACRCharacter* Candidate = Cast<ACRCharacter>(Overlap.GetActor());
			if (IsEnemy(Candidate))
			{
				const FVector To = Candidate->GetActorLocation() - From;
				if (FVector::DotProduct(To.GetSafeNormal2D(), Forward) > MissileSeekCone)
				{
					Candidates.AddUnique(TPair<float, ACRCharacter*>(To.SizeSquared(), Candidate));
				}
			}
		}
		Candidates.Sort([](const TPair<float, ACRCharacter*>& A, const TPair<float, ACRCharacter*>& B) { return A.Key < B.Key; });
		for (int32 i = 0; i < Candidates.Num() && i < Count; ++i)
		{
			MissileTargets.Add(Candidates[i].Value);
		}
		// 3) 적도 없으면 사거리 안 바닥
		const FVector Flat = AimPoint - From;
		if (Flat.Size2D() > MissileMaxGroundRange)
		{
			MissileTargetPoint = From + Flat.GetSafeNormal2D() * MissileMaxGroundRange - FVector(0.f, 0.f, 100.f);
		}
	}
	PendingMissiles = FMath::Max(Count, 1);
	MissileSalvoSize = PendingMissiles;
	MissileSalvoIndex = 0;
	MissileSpacing = 0.05f * (bSuper ? SuperMissileSpacingScale : 1.f);
	NextMissileTime = WeaponClock;
	UpdateMissileQueue();
}

void UCRWeaponComponent::UpdateMissileQueue()
{
	while (PendingMissiles > 0 && WeaponClock >= NextMissileTime)
	{
		LaunchOneMissile();
		--PendingMissiles;
		NextMissileTime += MissileSpacing;
	}
}

void UCRWeaponComponent::LaunchOneMissile()
{
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	if (!Pool || !WeaponRoot)
	{
		return;
	}
	const AActor* Owner = GetOwner();
	// 위 + 약간 뒤 + 좌우로 크게 부채꼴. 한 발씩 다른 방향
	const float Fan = MissileSalvoSize > 1 ? FMath::Lerp(-1.f, 1.f, float(MissileSalvoIndex) / (MissileSalvoSize - 1)) : 0.f;
	++MissileSalvoIndex;
	const FVector Direction = (FVector::UpVector * 1.f - Owner->GetActorForwardVector() * 0.1f
		+ Owner->GetActorRightVector() * (Fan * 1.3f + FMath::FRandRange(-0.2f, 0.2f)) + FMath::VRand() * 0.15f).GetSafeNormal();
	const FVector Start = GetMuzzleLocation();
	if (ACRMissile* Missile = Pool->Acquire<ACRMissile>(FTransform(Direction.Rotation(), Start)))
	{
		AActor* Target = MissileTargets.Num() > 0 ? MissileTargets[(MissileSalvoIndex - 1) % MissileTargets.Num()].Get() : nullptr;
		Missile->Launch(GetOwner(), Direction, Target, MissileTargetPoint);
	}
	PlayFireFeedback();
}

FVector UCRWeaponComponent::FireFlame()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const AActor* Owner = GetOwner();
	const ACRCharacter* Snowman = Cast<ACRCharacter>(Owner);
	FVector AimPoint = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 400.f;
	if (Snowman)
	{
		Snowman->GetWeaponAimPoint(AimPoint);
	}
	// 사거리 안으로 당기고, 그 지점 바닥에 세운다
	FVector Flat = AimPoint - Owner->GetActorLocation();
	if (Flat.Size2D() > Tuning.FlameRange)
	{
		AimPoint = Owner->GetActorLocation() + Flat.GetSafeNormal2D() * Tuning.FlameRange;
	}
	FVector Ground = AimPoint;
	FHitResult Hit;
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, AimPoint + FVector(0.f, 0.f, 300.f), AimPoint - FVector(0.f, 0.f, 2000.f), GroundTypes,
		FCollisionQueryParams(SCENE_QUERY_STAT(CRFlameGround), false, GetOwner())))
	{
		Ground = Hit.ImpactPoint;
	}
	if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
	{
		Fx->SpawnFlame(Ground, Tuning.FlameRadius, Tuning.FlameHeight, Tuning.FlameDuration);
	}
	PlayFireFeedback();
	return Ground;
}

void UCRWeaponComponent::PlayFireFeedback()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 총 킥백: 스프링에 속도 충격
	Recoil = FMath::Min(Recoil + 1.f, 2.f);
	RecoilVelocity = 0.f;

	ACRCharacter* Snowman = Cast<ACRCharacter>(GetOwner());
	if (Tuning.bCameraRecoil && Snowman && Snowman->GetViewComponent())
	{
		Snowman->GetViewComponent()->AddRecoil(Tuning.CameraRecoilPitch);
	}

	if (Tuning.bMuzzleFlash)
	{
		const FCRWeaponVisual* Visual = Visuals.Find(Equipped);
		const FLinearColor Color = Visual ? Visual->FlashColor : FLinearColor::White;
		const FVector Offset = Visual ? Visual->MuzzleOffset + FVector(6.f, 0.f, 0.f) : FVector::ZeroVector;

		MuzzleFlash->SetRelativeLocation(Offset);
		MuzzleFlash->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
		MuzzleFlash->SetRelativeScale3D(FVector(0.3f, 0.18f, 0.18f) * FMath::FRandRange(0.8f, 1.25f));
		MuzzleFlash->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, 1.f));
		MuzzleFlash->SetVisibility(true);

		MuzzleLight->SetRelativeLocation(Offset);
		MuzzleLight->SetLightColor(Color);
		MuzzleLight->SetIntensity(Tuning.MuzzleLightIntensity);
		MuzzleLight->SetVisibility(Tuning.MuzzleLightIntensity > 0.f);
		MuzzleFlashTimeLeft = Tuning.MuzzleFlashDuration;
	}
}

void UCRWeaponComponent::UpdateEffects(float RealDeltaTime)
{
	if (MuzzleFlashTimeLeft > 0.f)
	{
		MuzzleFlashTimeLeft -= RealDeltaTime;
		if (MuzzleFlashTimeLeft <= 0.f)
		{
			MuzzleFlash->SetVisibility(false);
			MuzzleLight->SetVisibility(false);
		}
	}

	if (LaserBeamTimeLeft > 0.f)
	{
		const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
		const float Fade = FMath::Clamp(LaserBeamTimeLeft / FMath::Max(Tuning.LaserBeamDuration, 0.001f), 0.f, 1.f);
		const FVector Delta = LaserBeamEnd - LaserBeamStart;
		const float Length = Delta.Size();
		const float Width = Tuning.LaserBeamWidth * Fade / 100.f;

		// 원기둥(Z축, 높이 100)을 시작~끝 사이에 늘림
		LaserBeam->SetWorldLocationAndRotation(LaserBeamStart + Delta * 0.5f, FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator());
		LaserBeam->SetWorldScale3D(FVector(Width, Width, Length / 100.f));
		LaserBeam->SetVisibility(Length > 1.f);

		LaserBeamTimeLeft -= RealDeltaTime;
		if (LaserBeamTimeLeft <= 0.f)
		{
			LaserBeam->SetVisibility(false);
		}
	}
}
