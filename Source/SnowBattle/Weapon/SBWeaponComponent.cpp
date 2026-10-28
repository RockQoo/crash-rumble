#include "Weapon/SBWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/SBViewComponent.h"
#include "Character/SBSnowmanCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SBActorPool.h"
#include "Core/SBGameSettings.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Feedback/SBFeedbackSubsystem.h"
#include "GAS/SBCombat.h"
#include "GAS/SBWeaponAbilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Weapon/SBProjectile.h"

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
	constexpr float GlowIntensity = 12.f;

	FVector SwordHandLocation(float Yaw)
	{
		const float Rad = FMath::DegreesToRadians(Yaw);
		return FVector(FMath::Cos(Rad) * SwingRadius, FMath::Sin(Rad) * SwingRadius, SwingHeight);
	}
}

USBWeaponComponent::USBWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USBWeaponComponent::Initialize(USceneComponent* InHand)
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
	Equip(ESBWeaponType::None);
}

void USBWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnSwingHit.Unbind();
	OnSwingFinished.Unbind();
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* USBWeaponComponent::MakePart(USceneComponent* Parent, const TCHAR* MeshPath, const FVector& Location,
	const FVector& Scale, const FRotator& Rotation, const FLinearColor& Color, bool bGlow, float Intensity)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(!bGlow);
	Part->SetupAttachment(Parent ? Parent : GetOwner()->GetRootComponent());
	Part->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Part->RegisterComponent();

	UMaterialInterface* Material = bGlow ? USBGameSettings::LoadGlowMaterial() : USBGameSettings::LoadCharacterMaterial();
	if (Material)
	{
		Part->SetMaterial(0, Material);
		Part->SetCustomPrimitiveDataVector4(SBMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, Color.A));
		Part->SetCustomPrimitiveDataFloat(SBMaterialData::FlashIndex, bGlow ? Intensity : 0.f);
	}
	return Part;
}

void USBWeaponComponent::BuildVisuals()
{
	const FRotator AlongX(-90.f, 0.f, 0.f); // 원기둥(Z축)을 X축 방향으로 눕힘

	// 칼
	{
		FSBWeaponVisual& Visual = Visuals.Add(ESBWeaponType::Sword);
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(0, 0, 0), FVector(0.05f, 0.05f, 0.2f), AlongX, FLinearColor(0.25f, 0.12f, 0.05f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(10, 0, 0), FVector(0.05f, 0.3f, 0.05f), FRotator::ZeroRotator, FLinearColor(1.f, 0.75f, 0.2f), false));
		SwordBlade = MakePart(WeaponRoot, CubeMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, FLinearColor(0.85f, 0.9f, 1.f), false);
		Visual.Parts.Add(SwordBlade);
		UpdateSwordBlade();
	}

	// 레이저총
	{
		FSBWeaponVisual& Visual = Visuals.Add(ESBWeaponType::Laser);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.32f, 0.12f, 0.16f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.15f, 0.18f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 6), FVector(0.07f, 0.07f, 0.28f), AlongX, FLinearColor(0.3f, 0.3f, 0.35f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(50, 0, 6), FVector(0.08f), FRotator::ZeroRotator, LaserColor, true, 4.f));
		Visual.MuzzleOffset = FVector(54, 0, 6);
		Visual.FlashColor = LaserColor;
	}

	// 투사체총
	{
		FSBWeaponVisual& Visual = Visuals.Add(ESBWeaponType::Projectile);
		Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.36f, 0.16f, 0.2f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.35f, 0.15f), false));
		Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 8), FVector(0.13f, 0.13f, 0.24f), AlongX, FLinearColor(0.2f, 0.2f, 0.2f), false));
		Visual.MuzzleOffset = FVector(50, 0, 8);
		Visual.FlashColor = ProjectileFlashColor;
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

void USBWeaponComponent::Equip(ESBWeaponType Type)
{
	Equipped = Type;
	for (TPair<ESBWeaponType, FSBWeaponVisual>& Pair : Visuals)
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
}

TSubclassOf<UGameplayAbility> USBWeaponComponent::GetAbilityClass(ESBWeaponType Type) const
{
	switch (Type)
	{
	case ESBWeaponType::Sword: return USBGA_SwordSwing::StaticClass();
	case ESBWeaponType::Laser: return USBGA_LaserFire::StaticClass();
	case ESBWeaponType::Projectile: return USBGA_ProjectileFire::StaticClass();
	default: return nullptr;
	}
}

void USBWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!WeaponRoot)
	{
		return;
	}

	WeaponClock += DeltaTime;

	if (Equipped == ESBWeaponType::Sword)
	{
		UpdateSwordBlade();
	}
	TryFire();
	UpdateSwing(DeltaTime);
	UpdateRecoil(DeltaTime);
	UpdateHandPose(DeltaTime);
	UpdateEffects(FApp::GetDeltaTime());
}

void USBWeaponComponent::TryFire()
{
	if (!bTriggerHeld || Equipped == ESBWeaponType::None || bSwinging || WeaponClock < NextFireTime)
	{
		return;
	}

	ASBSnowmanCharacter* Snowman = Cast<ASBSnowmanCharacter>(GetOwner());
	UAbilitySystemComponent* ASC = Snowman ? Snowman->GetAbilitySystemComponent() : nullptr;
	if (!ASC || Snowman->IsDead())
	{
		return;
	}

	if (ASC->TryActivateAbilityByClass(GetAbilityClass(Equipped)))
	{
		const FSBWeaponFeel Feel = SBWeapon::GetFeel(USBTuningSubsystem::GetSettings(this), Equipped);
		NextFireTime = WeaponClock + Feel.Interval;
	}
}

void USBWeaponComponent::StartSwing(FSBSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished)
{
	OnSwingHit = InOnHit;
	OnSwingFinished = InOnFinished;
	bSwinging = true;
	SwingTime = 0.f;
	RecoverTime = 0.f;
	SwingHitActors.Reset();
	PrevBladeSamples.Reset();
}

void USBWeaponComponent::UpdateSwing(float DeltaTime)
{
	if (RecoverTime > 0.f)
	{
		RecoverTime = FMath::Max(0.f, RecoverTime - DeltaTime);
	}
	if (!bSwinging)
	{
		return;
	}

	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
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

void USBWeaponComponent::SweepBlade(bool bFirstSample)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
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
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBSwordSweep), false, GetOwner());
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

void USBWeaponComponent::UpdateSwordBlade()
{
	if (SwordBlade)
	{
		const float Length = USBTuningSubsystem::GetSettings(this).SwordLength;
		SwordBlade->SetRelativeLocation(FVector(BladeStart + Length * 0.5f, 0.f, 0.f));
		SwordBlade->SetRelativeScale3D(FVector(Length / 100.f, 0.08f, 0.02f));
	}
}

void USBWeaponComponent::UpdateHandPose(float DeltaTime)
{
	if (bSwinging)
	{
		return; // UpdateSwing에서 이미 설정
	}

	FVector TargetLocation;
	FRotator TargetRotation;

	if (Equipped == ESBWeaponType::Sword)
	{
		TargetLocation = SwordHandLocation(IdleYaw);
		TargetRotation = FRotator(IdlePitch, IdleYaw, 0.f);
	}
	else if (Equipped == ESBWeaponType::Laser || Equipped == ESBWeaponType::Projectile)
	{
		// 조준점을 향하도록 손을 돌린다
		const AActor* Owner = GetOwner();
		const ASBSnowmanCharacter* Snowman = Cast<ASBSnowmanCharacter>(Owner);
		const FTransform ActorTransform = Owner->GetActorTransform();
		FRotator Aim = FRotator::ZeroRotator;
		if (Snowman && Snowman->GetViewComponent() && Snowman->IsPlayerControlled())
		{
			const FVector HandWorld = ActorTransform.TransformPosition(GunHandLocation);
			const FVector LocalDir = ActorTransform.InverseTransformVectorNoScale(Snowman->GetViewComponent()->GetAimPoint() - HandWorld);
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

void USBWeaponComponent::UpdateRecoil(float DeltaTime)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

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

bool USBWeaponComponent::GetHandPose(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!bHasHandPose)
	{
		return false;
	}

	OutLocation = HandLocation;
	OutRotation = HandRotation;

	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	if (Tuning.bWeaponRecoil && Recoil != 0.f)
	{
		OutLocation -= HandRotation.Vector() * Recoil * Tuning.RecoilKickback;
		OutLocation += RecoilJitterOffset;
		OutRotation.Pitch += Recoil * Tuning.RecoilPitch;
	}
	return true;
}

FVector USBWeaponComponent::GetMuzzleLocation() const
{
	const FSBWeaponVisual* Visual = Visuals.Find(Equipped);
	return WeaponRoot->GetComponentTransform().TransformPosition(Visual ? Visual->MuzzleOffset : FVector::ZeroVector);
}

FVector USBWeaponComponent::GetFireDirection(const FVector& Start) const
{
	const ASBSnowmanCharacter* Snowman = Cast<ASBSnowmanCharacter>(GetOwner());
	if (Snowman && Snowman->IsPlayerControlled() && Snowman->GetViewComponent())
	{
		const FVector ToAim = Snowman->GetViewComponent()->GetAimPoint() - Start;
		// 조준점이 총구 뒤/너무 가까우면 몸 정면으로
		if (ToAim.SizeSquared() > FMath::Square(50.f) && FVector::DotProduct(ToAim, GetOwner()->GetActorForwardVector()) > 0.f)
		{
			return ToAim.GetSafeNormal();
		}
	}
	return WeaponRoot->GetForwardVector();
}

bool USBWeaponComponent::FireLaser(FHitResult& OutHit)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	const FVector Start = GetMuzzleLocation();
	const FVector End = Start + GetFireDirection(Start) * Tuning.LaserRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBLaser), false, GetOwner());
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, SBCombat::GetWeaponQueryParams(), Params);

	LaserBeamStart = Start;
	LaserBeamEnd = bHit ? FVector(OutHit.ImpactPoint) : End;
	LaserBeamTimeLeft = Tuning.LaserBeamDuration;

	// 벽/바닥에 맞으면 작은 불꽃
	if (bHit && !Cast<ASBSnowmanCharacter>(OutHit.GetActor()))
	{
		if (USBFeedbackSubsystem* Feedback = USBFeedbackSubsystem::Get(this))
		{
			Feedback->SpawnImpactParticles(OutHit.ImpactPoint, OutHit.ImpactNormal, LaserColor, 0.5f);
		}
	}

	PlayFireFeedback();
	return bHit && Cast<ASBSnowmanCharacter>(OutHit.GetActor()) != nullptr;
}

void USBWeaponComponent::FireProjectile()
{
	USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this);
	if (!Pool)
	{
		return;
	}
	const FVector Start = GetMuzzleLocation();
	const FVector Direction = GetFireDirection(Start);
	if (ASBProjectile* Projectile = Pool->Acquire<ASBProjectile>(FTransform(Direction.Rotation(), Start)))
	{
		Projectile->Launch(GetOwner(), Direction);
	}
	PlayFireFeedback();
}

void USBWeaponComponent::PlayFireFeedback()
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

	// 총 킥백: 스프링에 속도 충격
	Recoil = FMath::Min(Recoil + 1.f, 2.f);
	RecoilVelocity = 0.f;

	ASBSnowmanCharacter* Snowman = Cast<ASBSnowmanCharacter>(GetOwner());
	if (Tuning.bCameraRecoil && Snowman && Snowman->GetViewComponent())
	{
		Snowman->GetViewComponent()->AddRecoil(Tuning.CameraRecoilPitch);
	}

	if (Tuning.bMuzzleFlash)
	{
		const FSBWeaponVisual* Visual = Visuals.Find(Equipped);
		const FLinearColor Color = Visual ? Visual->FlashColor : FLinearColor::White;
		const FVector Offset = Visual ? Visual->MuzzleOffset + FVector(6.f, 0.f, 0.f) : FVector::ZeroVector;

		MuzzleFlash->SetRelativeLocation(Offset);
		MuzzleFlash->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
		MuzzleFlash->SetRelativeScale3D(FVector(0.3f, 0.18f, 0.18f) * FMath::FRandRange(0.8f, 1.25f));
		MuzzleFlash->SetCustomPrimitiveDataVector4(SBMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, 1.f));
		MuzzleFlash->SetVisibility(true);

		MuzzleLight->SetRelativeLocation(Offset);
		MuzzleLight->SetLightColor(Color);
		MuzzleLight->SetIntensity(Tuning.MuzzleLightIntensity);
		MuzzleLight->SetVisibility(Tuning.MuzzleLightIntensity > 0.f);
		MuzzleFlashTimeLeft = Tuning.MuzzleFlashDuration;
	}
}

void USBWeaponComponent::UpdateEffects(float RealDeltaTime)
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
		const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
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
