#include "Weapon/CRWeaponComponent.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
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
	const FLinearColor SwordBladeColor(0.85f, 0.9f, 1.f);

	// 미사일 유도 표적: 커서 근처(300cm)에 적이 있으면 그 적, 없으면 앞쪽 일정 거리 안의 적들에게 나눠 보낸다
	constexpr float MissileLockRadius = 300.f;
	constexpr float MissileSeekCone = 0.3f;
	constexpr float SalvoSpacing = 0.05f;
	constexpr float SingleSpacing = 0.12f;  // 일제 사격이 아닌 여러 발 간격
	constexpr float SuperMissileSpacingScale = 0.5f;
	constexpr float MissileScatter = 0.8f;   // 두 번째 발부터 조준점 주변(폭발 반경 비율)으로 흩뿌림
	constexpr float PassiveLaunchHeight = 120.f;
	constexpr float GlowIntensity = 12.f;
	constexpr int32 MaxBeams = 24;

	FVector SwordHandLocation(float Yaw)
	{
		const float Rad = FMath::DegreesToRadians(Yaw);
		return FVector(FMath::Cos(Rad) * SwingRadius, FMath::Sin(Rad) * SwingRadius, SwingHeight);
	}

	/** 여러 발일 때 k번째 발의 좌우 각도 (Count발을 Spread도 안에 고르게). */
	float SpreadYaw(int32 Index, int32 Count, float Spread)
	{
		return Count > 1 ? FMath::Lerp(-Spread * 0.5f, Spread * 0.5f, float(Index) / (Count - 1)) : 0.f;
	}

	bool TraceGround(const UWorld* World, const FVector& Point, const AActor* Ignore, FVector& OutGround)
	{
		FHitResult Hit;
		FCollisionObjectQueryParams GroundTypes;
		GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
		GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (World->LineTraceSingleByObjectType(Hit, Point + FVector(0.f, 0.f, 300.f), Point - FVector(0.f, 0.f, 2000.f), GroundTypes,
			FCollisionQueryParams(SCENE_QUERY_STAT(CRWeaponGround), false, Ignore)))
		{
			OutGround = Hit.ImpactPoint;
			return true;
		}
		OutGround = Point;
		return false;
	}
}

UCRWeaponComponent::UCRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCRWeaponComponent::Initialize(USceneComponent* InHand, USceneComponent* InOffHand)
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

	if (InOffHand)
	{
		OffHandRoot = NewObject<USceneComponent>(Owner, TEXT("OffHandRoot"));
		OffHandRoot->SetupAttachment(InOffHand);
		OffHandRoot->SetUsingAbsoluteScale(true);
		OffHandRoot->RegisterComponent();
	}

	BuildVisuals();
	Equip(ECRWeaponType::None);
}

void UCRWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnSwingHit.Unbind();
	OnSwingFinished.Unbind();
	Super::EndPlay(EndPlayReason);
}

FCRSkillStats UCRWeaponComponent::GetStats(ECRWeaponType Family, ECRSkillForm Form) const
{
	return CRSkill::GetStats(GetOwner(), Family, Form);
}

bool UCRWeaponComponent::IsDualActive() const
{
	return Equipped == ECRWeaponType::Sword && OffHandRoot && GetStats(ECRWeaponType::Sword, ECRSkillForm::Active).GetFlag(ECRStat::Dual);
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
	// 무기별 외형은 처음 장착할 때 만든다 (BuildVisual). 고정 무기 AI가 안 쓰는 무기 파츠까지
	// 손에 붙이고 있으면 손이 움직일 때마다 숨은 파츠까지 위치 갱신 비용을 낸다
	MuzzleFlash = MakePart(WeaponRoot, SphereMesh, FVector::ZeroVector, FVector(0.3f, 0.18f, 0.18f), FRotator::ZeroRotator, FLinearColor::White, true, GlowIntensity);
	MuzzleFlash->SetVisibility(false);

	MuzzleLight = NewObject<UPointLightComponent>(GetOwner());
	MuzzleLight->SetupAttachment(WeaponRoot);
	MuzzleLight->SetAttenuationRadius(400.f);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->SetVisibility(false);
	MuzzleLight->RegisterComponent();
}

void UCRWeaponComponent::BuildOffHandVisual()
{
	if (!OffHandRoot || OffHandParts.Num() > 0)
	{
		return;
	}
	const FRotator AlongX(-90.f, 0.f, 0.f);
	OffHandParts.Add(MakePart(OffHandRoot, CylinderMesh, FVector(0, 0, 0), FVector(0.05f, 0.05f, 0.2f), AlongX, FLinearColor(0.25f, 0.12f, 0.05f), false));
	OffHandParts.Add(MakePart(OffHandRoot, CubeMesh, FVector(10, 0, 0), FVector(0.05f, 0.3f, 0.05f), FRotator::ZeroRotator, FLinearColor(1.f, 0.75f, 0.2f), false));
	OffHandBlade = MakePart(OffHandRoot, CubeMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, SwordBladeColor, false);
	OffHandParts.Add(OffHandBlade);
}

void UCRWeaponComponent::BuildVisual(ECRWeaponType Type)
{
	if (Type == ECRWeaponType::None || Visuals.Contains(Type) || !WeaponRoot)
	{
		return;
	}
	const FRotator AlongX(-90.f, 0.f, 0.f); // 원기둥(Z축)을 X축 방향으로 눕힘
	switch (Type)
	{
	case ECRWeaponType::Sword:
		// 칼
		{
			FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Sword);
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(0, 0, 0), FVector(0.05f, 0.05f, 0.2f), AlongX, FLinearColor(0.25f, 0.12f, 0.05f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(10, 0, 0), FVector(0.05f, 0.3f, 0.05f), FRotator::ZeroRotator, FLinearColor(1.f, 0.75f, 0.2f), false));
			SwordBlade = MakePart(WeaponRoot, CubeMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, SwordBladeColor, false);
			Visual.Parts.Add(SwordBlade);
			UpdateSwordBlade();
		}
		break;
	case ECRWeaponType::Laser:
		// 레이저총
		{
			FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Laser);
			Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.32f, 0.12f, 0.16f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.15f, 0.18f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 6), FVector(0.07f, 0.07f, 0.28f), AlongX, FLinearColor(0.3f, 0.3f, 0.35f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(50, 0, 6), FVector(0.08f), FRotator::ZeroRotator, LaserColor, true, 4.f));
			Visual.MuzzleOffset = FVector(54, 0, 6);
			Visual.FlashColor = LaserColor;
		}
		break;
	case ECRWeaponType::Projectile:
		// 투사체총
		{
			FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Projectile);
			Visual.Parts.Add(MakePart(WeaponRoot, CubeMesh, FVector(12, 0, 4), FVector(0.36f, 0.16f, 0.2f), FRotator::ZeroRotator, FLinearColor(0.15f, 0.35f, 0.15f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(36, 0, 8), FVector(0.13f, 0.13f, 0.24f), AlongX, FLinearColor(0.2f, 0.2f, 0.2f), false));
			Visual.MuzzleOffset = FVector(50, 0, 8);
			Visual.FlashColor = ProjectileFlashColor;
		}
		break;
	case ECRWeaponType::Lightning:
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
		break;
	case ECRWeaponType::Missile:
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
		break;
	case ECRWeaponType::Flame:
		// 불꽃 발사기: 통 + 분홍 노즐
		{
			FCRWeaponVisual& Visual = Visuals.Add(ECRWeaponType::Flame);
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(8, 0, 6), FVector(0.14f, 0.14f, 0.3f), AlongX, FLinearColor(0.6f, 0.15f, 0.4f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, CylinderMesh, FVector(32, 0, 6), FVector(0.06f, 0.06f, 0.22f), AlongX, FLinearColor(0.25f, 0.25f, 0.3f), false));
			Visual.Parts.Add(MakePart(WeaponRoot, SphereMesh, FVector(44, 0, 6), FVector(0.08f), FRotator::ZeroRotator, FlameGunColor, true, 4.f));
			Visual.MuzzleOffset = FVector(48, 0, 6);
			Visual.FlashColor = FlameGunColor;
		}
		break;
	default:
		break;
	}
}

void UCRWeaponComponent::Equip(ECRWeaponType Type)
{
	const bool bChanged = Equipped != Type;
	Equipped = Type;
	BuildVisual(Type);
	for (TPair<ECRWeaponType, FCRWeaponVisual>& Pair : Visuals)
	{
		for (UStaticMeshComponent* Part : Pair.Value.Parts)
		{
			Part->SetVisibility(Pair.Key == Type);
		}
	}
	RefreshFromStats();
	if (!bChanged)
	{
		return; // 같은 무기 재장착(같은 버튼 재입력, 장비 갱신): 스윙·발사 대기시간 유지
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
				? WeaponRoot->GetComponentTransform().TransformPosition(FVector(BladeStart + BladeLength, 0.f, 0.f))
				: GetMuzzleLocation();
			Fx->SpawnSparkle(Tip, UCRTuningSubsystem::GetSettings(this).SparkleSize, WeaponRoot);
		}
	}
}

void UCRWeaponComponent::RefreshFromStats()
{
	const bool bDual = IsDualActive();
	if (bDual)
	{
		BuildOffHandVisual();
	}
	for (UStaticMeshComponent* Part : OffHandParts)
	{
		if (Part)
		{
			Part->SetVisibility(bDual);
		}
	}
	UpdateSwordBlade();
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
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_WeaponTick);
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
		const FCRSkillStats Stats = GetStats(Equipped, ECRSkillForm::Active);
		// 웨이브 몬스터는 플레이어 무기보다 느리게 쏜다
		const float Scale = Snowman->GetTeam() == ECRTeam::Monster ? UCRTuningSubsystem::GetSettings(this).WaveFireIntervalScale : 1.f;
		NextFireTime = WeaponClock + CRSkill::GetAttackInterval(Snowman, FMath::Max(Stats.Get(ECRStat::Interval), 0.02f)) * Scale;
	}
}

void UCRWeaponComponent::StartSwing(FCRSwingHitDelegate InOnHit, FSimpleDelegate InOnFinished)
{
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Sword, ECRSkillForm::Active);
	SwingDuration = FMath::Max(Stats.Get(ECRStat::SwingTime), 0.01f);
	SwingArc = FMath::Clamp(Stats.Get(ECRStat::Arc), 10.f, 360.f);
	OnSwingHit = InOnHit;
	OnSwingFinished = InOnFinished;
	bSwinging = true;
	SwingTime = 0.f;
	RecoverTime = 0.f;
	SwingHitActors.Reset();
	PrevBladeSamples.Reset();
	PrevOffBladeSamples.Reset();
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

	const bool bFirst = SwingTime == 0.f;
	SwingTime += DeltaTime;

	const float Alpha = FMath::Clamp(SwingTime / SwingDuration, 0.f, 1.f);
	const float Eased = 1.f - FMath::Pow(1.f - Alpha, 3.f); // 빠르게 나가서 끝에서 감속
	const float HalfArc = SwingArc * 0.5f; // 360 = 몸을 한 바퀴 도는 회전 베기
	const float Yaw = FMath::Lerp(HalfArc, -HalfArc, Eased);

	HandLocation = SwordHandLocation(Yaw);
	HandRotation = FRotator(FMath::Lerp(10.f, -5.f, Eased), Yaw, 0.f);
	bHasHandPose = true;

	// 손 위치를 이번 프레임 값으로 반영한 뒤 칼날 스윕 (쌍검이면 왼손도 좌우 반대로)
	if (USceneComponent* Hand = WeaponRoot->GetAttachParent())
	{
		Hand->SetRelativeLocationAndRotation(HandLocation, HandRotation);
	}
	SweepBlade(WeaponRoot, PrevBladeSamples, bFirst);
	if (IsDualActive())
	{
		FVector OffLocation;
		FRotator OffRotation;
		if (GetOffHandPose(OffLocation, OffRotation))
		{
			if (USceneComponent* OffHand = OffHandRoot->GetAttachParent())
			{
				OffHand->SetRelativeLocationAndRotation(OffLocation, OffRotation);
			}
			SweepBlade(OffHandRoot, PrevOffBladeSamples, bFirst);
		}
	}

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

void UCRWeaponComponent::SweepBlade(USceneComponent* Root, TArray<FVector>& PrevSamples, bool bFirstSample)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FTransform RootTransform = Root->GetComponentTransform();

	TArray<FVector> Samples;
	Samples.Reserve(BladeSampleCount);
	for (int32 i = 0; i < BladeSampleCount; ++i)
	{
		const float T = (i + 1.f) / BladeSampleCount;
		Samples.Add(RootTransform.TransformPosition(FVector(BladeStart + BladeLength * T, 0.f, 0.f)));
	}
	if (bFirstSample || PrevSamples.Num() != Samples.Num())
	{
		PrevSamples = Samples;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRSwordSweep), false, GetOwner());
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Tuning.SwordHitRadius);

	for (int32 i = 0; i < Samples.Num(); ++i)
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, PrevSamples[i], Samples[i], FQuat::Identity, ObjectParams, Shape, Params);
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
	PrevSamples = Samples;
}

void UCRWeaponComponent::UpdateSwordBlade()
{
	BladeLength = GetStats(ECRWeaponType::Sword, ECRSkillForm::Active).Get(ECRStat::Length);
	for (UStaticMeshComponent* Blade : { SwordBlade.Get(), OffHandBlade.Get() })
	{
		if (Blade)
		{
			Blade->SetRelativeLocation(FVector(BladeStart + BladeLength * 0.5f, 0.f, 0.f));
			Blade->SetRelativeScale3D(FVector(BladeLength / 100.f, 0.08f, 0.02f));
		}
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

bool UCRWeaponComponent::GetOffHandPose(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!bHasHandPose || !IsDualActive())
	{
		return false;
	}
	// 오른손 자세를 몸 가운데 면(XZ)에 대해 뒤집는다
	OutLocation = FVector(HandLocation.X, -HandLocation.Y, HandLocation.Z);
	OutRotation = FRotator(HandRotation.Pitch, -HandRotation.Yaw, -HandRotation.Roll);
	return true;
}

FVector UCRWeaponComponent::GetMuzzleLocation() const
{
	const FCRWeaponVisual* Visual = Visuals.Find(Equipped);
	return WeaponRoot->GetComponentTransform().TransformPosition(Visual ? Visual->MuzzleOffset : FVector::ZeroVector);
}

FVector UCRWeaponComponent::GetAimPoint() const
{
	const AActor* Owner = GetOwner();
	FVector AimPoint = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 1000.f;
	if (const ACRCharacter* Snowman = Cast<ACRCharacter>(Owner))
	{
		Snowman->GetWeaponAimPoint(AimPoint);
	}
	return AimPoint;
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

void UCRWeaponComponent::FireLaser()
{
	const FVector Start = GetMuzzleLocation();
	ShootLaser(Start, GetFireDirection(Start), ECRSkillForm::Active);
	PlayFireFeedback();
}

void UCRWeaponComponent::ShootLaser(const FVector& Start, const FVector& Direction, ECRSkillForm Form)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Laser, Form);
	AActor* Owner = GetOwner();
	const int32 BeamCount = FMath::Clamp(Stats.GetInt(ECRStat::Count), 1, 12);
	const int32 MaxTargets = FMath::Max(Stats.GetInt(ECRStat::Pierce), 0) + 1;
	const float Width = Tuning.LaserBeamWidth * FMath::Max(Stats.Get(ECRStat::Size), 0.1f);
	constexpr int32 MaxPassThrough = 16; // 아군/시체를 통과하는 최대 횟수

	for (int32 Beam = 0; Beam < BeamCount; ++Beam)
	{
		const FVector Dir = Direction.RotateAngleAxis(SpreadYaw(Beam, BeamCount, Stats.Get(ECRStat::Spread)), FVector::UpVector);
		const FVector End = Start + Dir * Stats.Get(ECRStat::Range);

		// 눈사람을 맞히면 그 대상을 빼고 다시 쏜다 (관통 수만큼)
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CRLaser), false, Owner);
		FVector BeamEnd = End;
		int32 Hits = 0;
		for (int32 Iteration = 0; Iteration < MaxTargets + MaxPassThrough; ++Iteration)
		{
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, CRCombat::GetWeaponQueryParams(), Params))
			{
				break;
			}
			ACRCharacter* HitSnowman = Cast<ACRCharacter>(Hit.GetActor());
			if (!HitSnowman)
			{
				// 벽/바닥: 빔이 여기서 끝나고 작은 불꽃 (블럭이면 HP 1)
				BeamEnd = Hit.ImpactPoint;
				CRCombat::DamageWorldByGun(Hit, Owner);
				if (UCRFeedbackSubsystem* Feedback = UCRFeedbackSubsystem::Get(this))
				{
					Feedback->SpawnImpactParticles(Hit.ImpactPoint, Hit.ImpactNormal, LaserColor, 0.5f);
				}
				break;
			}
			// 쓰러진 눈사람과 아군(같은 팀, 도플갱어)은 빔이 통과
			if (HitSnowman->IsDead() || !CRCombat::IsHostile(Owner, HitSnowman))
			{
				Params.AddIgnoredActor(HitSnowman);
				continue;
			}
			CRCombat::ApplyWeaponHit(Owner, HitSnowman, ECRWeaponType::Laser, Hit, Form);
			if (++Hits >= MaxTargets)
			{
				BeamEnd = Hit.ImpactPoint;
				break;
			}
			Params.AddIgnoredActor(HitSnowman);
		}
		AddBeam(Start, BeamEnd, Width);
	}
}

void UCRWeaponComponent::AddBeam(const FVector& Start, const FVector& End, float Width)
{
	if (Beams.Num() >= MaxBeams)
	{
		Beams.RemoveAt(0, 1, EAllowShrinking::No);
	}
	FBeamFx& Beam = Beams.AddDefaulted_GetRef();
	Beam.Start = Start;
	Beam.End = End;
	Beam.Width = Width;
	Beam.TimeLeft = UCRTuningSubsystem::GetSettings(this).LaserBeamDuration;
}

void UCRWeaponComponent::FireProjectile()
{
	const FVector Start = GetMuzzleLocation();
	ShootProjectiles(Start, GetFireDirection(Start), ECRSkillForm::Active);
	PlayFireFeedback();
}

void UCRWeaponComponent::ShootProjectiles(const FVector& Start, const FVector& Direction, ECRSkillForm Form)
{
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	if (!Pool)
	{
		return;
	}
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Projectile, Form);
	const int32 Pellets = FMath::Clamp(Stats.GetInt(ECRStat::Count), 1, 20);
	const float Spread = Stats.Get(ECRStat::Spread);
	for (int32 i = 0; i < Pellets; ++i)
	{
		// 산탄: 부채꼴로 고르게 + 살짝 흔들림
		const float Jitter = Pellets > 1 ? FMath::FRandRange(-0.15f, 0.15f) * Spread / Pellets : 0.f;
		FVector Dir = Direction.RotateAngleAxis(SpreadYaw(i, Pellets, Spread) + Jitter, FVector::UpVector);
		if (Pellets > 1)
		{
			Dir = (Dir + FVector(0.f, 0.f, FMath::FRandRange(-0.03f, 0.03f))).GetSafeNormal();
		}
		if (ACRProjectile* Projectile = Pool->Acquire<ACRProjectile>(FTransform(Dir.Rotation(), Start)))
		{
			Projectile->Launch(GetOwner(), Dir, Form);
		}
	}
}

void UCRWeaponComponent::SetHandOverride(const FVector& Location, const FRotator& Rotation, float Duration)
{
	OverrideHandLocation = Location;
	OverrideHandRotation = Rotation;
	OverrideTimeLeft = Duration;
}

void UCRWeaponComponent::FireLightning()
{
	const FVector Start = GetMuzzleLocation();
	ShootLightning(Start, Start + GetFireDirection(Start) * 100.f, ECRSkillForm::Active);
	PlayFireFeedback();
}

void UCRWeaponComponent::ShootLightning(const FVector& Start, const FVector& AimPoint, ECRSkillForm Form)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Lightning, Form);
	AActor* Owner = GetOwner();
	const FVector Direction = (AimPoint - Start).GetSafeNormal();
	const FVector End = Start + Direction * FMath::Max(Stats.Get(ECRStat::Range), FVector::Dist(Start, AimPoint) + 100.f);
	UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRLightning), false, Owner);
	FVector BoltEnd = End;
	ACRCharacter* FirstTarget = nullptr;
	for (int32 Iteration = 0; Iteration < 16; ++Iteration)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, CRCombat::GetWeaponQueryParams(), Params))
		{
			break;
		}
		ACRCharacter* HitSnowman = Cast<ACRCharacter>(Hit.GetActor());
		if (HitSnowman && (HitSnowman->IsDead() || !CRCombat::IsHostile(Owner, HitSnowman)))
		{
			Params.AddIgnoredActor(HitSnowman);
			continue;
		}
		// 끝점은 대상 중심이 아니라 표면 (중심이면 끝 섬광이 몸 안에 묻혀 목에 링처럼 보인다)
		BoltEnd = Hit.ImpactPoint;
		if (HitSnowman)
		{
			FirstTarget = HitSnowman;
			CRCombat::ApplyWeaponHit(Owner, HitSnowman, ECRWeaponType::Lightning, Hit, Form);
		}
		else
		{
			CRCombat::DamageWorld(Hit, Tuning.LightningBlockDamage, Owner);
			// 바닥에 맞으면 큐브 잔해 8개
			if (Fx && Tuning.bCubeDebris)
			{
				Fx->SpawnDebris(Hit.ImpactPoint, Hit.ImpactNormal, 8, FLinearColor(0.55f, 0.55f, 0.6f), FLinearColor(0.35f, 0.33f, 0.4f), 1.f, Hit.ImpactPoint.Z - 1.f);
			}
		}
		break;
	}
	if (Fx)
	{
		Fx->SpawnLightning(Start, BoltEnd, Tuning.LightningBoltDuration);
	}

	// 체인 라이트닝: 맞은 대상에서 가까운 다음 적으로 튕긴다 (치지직)
	const int32 Chain = FMath::Clamp(Stats.GetInt(ECRStat::Chain), 0, 20);
	TArray<ACRCharacter*> Struck;
	ACRCharacter* From = FirstTarget;
	if (From)
	{
		Struck.Add(From);
	}
	for (int32 i = 0; i < Chain && From; ++i)
	{
		const FVector FromPoint = From->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		ACRCharacter* Next = CRCombat::FindNearestHostile(Owner, FromPoint, Stats.Get(ECRStat::ChainRange), &Struck);
		if (!Next)
		{
			break;
		}
		const FVector ToPoint = Next->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		if (Fx)
		{
			Fx->SpawnLightning(FromPoint, ToPoint, Tuning.LightningBoltDuration * 0.8f);
		}
		FHitResult Hit(Next, nullptr, ToPoint, (FromPoint - ToPoint).GetSafeNormal());
		Hit.bBlockingHit = true;
		CRCombat::ApplyWeaponHitFrom(Owner, Next, ECRWeaponType::Lightning, Hit, FromPoint, -1.f, Form);
		Struck.Add(Next);
		From = Next;
	}
}

void UCRWeaponComponent::FireMissiles(int32 CountOverride, bool bSuper)
{
	ShootMissiles(GetAimPoint(), ECRSkillForm::Active, CountOverride, bSuper);
}

void UCRWeaponComponent::ShootMissiles(const FVector& InAimPoint, ECRSkillForm Form, int32 CountOverride, bool bSuper)
{
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Missile, Form);
	const ACRCharacter* Snowman = Cast<ACRCharacter>(GetOwner());
	const AActor* Owner = GetOwner();
	const FVector From = Owner->GetActorLocation();
	const int32 Count = FMath::Clamp(CountOverride >= 0 ? CountOverride : Stats.GetInt(ECRStat::Count), 1, 64);
	const bool bHoming = bSuper || Stats.GetFlag(ECRStat::Homing);
	const bool bSalvo = bSuper || Stats.GetFlag(ECRStat::Salvo);

	// 사거리 안으로 당기고 바닥에 붙인다
	FVector AimPoint = InAimPoint;
	const FVector Flat = AimPoint - From;
	const float Range = Stats.Get(ECRStat::Range);
	if (Flat.Size2D() > Range)
	{
		AimPoint = From + Flat.GetSafeNormal2D() * Range;
		AimPoint.Z = InAimPoint.Z;
	}
	FVector Ground;
	TraceGround(GetWorld(), AimPoint, Owner, Ground);

	// 유도: 1) 커서 근처(300cm)의 적 하나 2) 없으면 앞쪽 사거리 안의 가까운 적들에게 나눠 보낸다
	TArray<TWeakObjectPtr<AActor>> Targets;
	if (bHoming)
	{
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
			if (Candidate && !Candidate->IsDead() && !Candidate->IsInPool() && !Candidate->IsHidden() && CRCombat::IsHostile(Owner, Candidate)
				&& Distance < Best)
			{
				Best = Distance;
				Locked = Candidate;
			}
		}
		if (Locked)
		{
			Targets.Add(Locked);
		}
		else
		{
			TArray<FOverlapResult> Nearby;
			GetWorld()->OverlapMultiByObjectType(Nearby, From, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Range),
				FCollisionQueryParams(SCENE_QUERY_STAT(CRMissileSeek), false, Owner));
			TArray<TPair<float, ACRCharacter*>> Candidates;
			const FVector Forward = (AimPoint - From).GetSafeNormal2D();
			for (const FOverlapResult& Overlap : Nearby)
			{
				ACRCharacter* Candidate = Cast<ACRCharacter>(Overlap.GetActor());
				if (Candidate && !Candidate->IsDead() && !Candidate->IsInPool() && !Candidate->IsHidden() && CRCombat::IsHostile(Owner, Candidate))
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
				Targets.Add(Candidates[i].Value);
			}
		}
	}

	const float Spacing = bSalvo ? SalvoSpacing * (bSuper ? SuperMissileSpacingScale : 1.f) : SingleSpacing;
	const float Radius = Stats.Get(ECRStat::Radius);
	const float FirstLaunch = PendingMissiles.Num() > 0 ? FMath::Max(PendingMissiles.Last().LaunchTime + Spacing, WeaponClock) : WeaponClock;
	for (int32 i = 0; i < Count; ++i)
	{
		FPendingMissile& Pending = PendingMissiles.AddDefaulted_GetRef();
		Pending.Target = Targets.Num() > 0 ? Targets[i % Targets.Num()] : nullptr;
		// 유도 대상이 없으면 조준점 (두 번째 발부터 주변에 흩뿌림)
		FVector Point = Ground;
		if (i > 0)
		{
			const FVector2D Offset = FMath::RandPointInCircle(Radius * MissileScatter);
			TraceGround(GetWorld(), Ground + FVector(Offset.X, Offset.Y, 0.f), Owner, Point);
		}
		Pending.TargetPoint = Point;
		Pending.Form = Form;
		Pending.bSalvo = bSalvo;
		Pending.SalvoIndex = i;
		Pending.SalvoSize = Count;
		Pending.LaunchTime = FirstLaunch + i * Spacing;
	}
	UpdateMissileQueue();
}

void UCRWeaponComponent::CancelActions()
{
	if (bSwinging)
	{
		bSwinging = false;
		OnSwingFinished.ExecuteIfBound();
	}
	PendingMissiles.Reset();
}

void UCRWeaponComponent::UpdateMissileQueue()
{
	int32 Launched = 0;
	while (Launched < PendingMissiles.Num() && WeaponClock >= PendingMissiles[Launched].LaunchTime)
	{
		LaunchMissile(PendingMissiles[Launched]);
		++Launched;
	}
	if (Launched > 0)
	{
		PendingMissiles.RemoveAt(0, Launched, EAllowShrinking::No);
	}
}

void UCRWeaponComponent::LaunchMissile(const FPendingMissile& Pending)
{
	UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	const AActor* Owner = GetOwner();
	if (!Pool || !WeaponRoot || !Owner)
	{
		return;
	}
	const bool bFromHand = Pending.Form == ECRSkillForm::Active && Equipped == ECRWeaponType::Missile;
	const FVector Start = bFromHand ? GetMuzzleLocation() : Owner->GetActorLocation() + FVector(0.f, 0.f, PassiveLaunchHeight);
	FVector Direction;
	if (Pending.bSalvo)
	{
		// 이타노 서커스: 위 + 약간 뒤 + 좌우로 크게 부채꼴, 한 발씩 다른 방향
		const float Fan = Pending.SalvoSize > 1 ? FMath::Lerp(-1.f, 1.f, float(Pending.SalvoIndex) / (Pending.SalvoSize - 1)) : 0.f;
		Direction = (FVector::UpVector * 1.f - Owner->GetActorForwardVector() * 0.1f
			+ Owner->GetActorRightVector() * (Fan * 1.3f + FMath::FRandRange(-0.2f, 0.2f)) + FMath::VRand() * 0.15f).GetSafeNormal();
	}
	else
	{
		// 기본: 조준 지점 쪽으로 높게 쏘아 올려 포물선처럼 떨어진다
		const FVector Goal = Pending.Target.IsValid() ? Pending.Target->GetActorLocation() : Pending.TargetPoint;
		Direction = ((Goal - Start).GetSafeNormal2D() + FVector(0.f, 0.f, 1.1f)).GetSafeNormal();
	}
	if (ACRMissile* Missile = Pool->Acquire<ACRMissile>(FTransform(Direction.Rotation(), Start)))
	{
		Missile->Launch(GetOwner(), Direction, Pending.Target.Get(), Pending.TargetPoint, Pending.Form);
	}
	if (bFromHand)
	{
		PlayFireFeedback();
	}
}

void UCRWeaponComponent::FireFlame()
{
	const AActor* Owner = GetOwner();
	const FVector AimPoint = GetAimPoint();
	FVector Direction = (AimPoint - Owner->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Owner->GetActorForwardVector();
	}
	ShootFlame(AimPoint, Direction, ECRSkillForm::Active);
	PlayFireFeedback();
}

void UCRWeaponComponent::ShootFlame(const FVector& InAimPoint, const FVector& Direction, ECRSkillForm Form)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FCRSkillStats Stats = GetStats(ECRWeaponType::Flame, Form);
	AActor* Owner = GetOwner();
	const FVector From = Owner->GetActorLocation();

	// 사거리 안으로 당긴다
	FVector AimPoint = InAimPoint;
	const FVector Flat = AimPoint - From;
	if (Flat.Size2D() > Stats.Get(ECRStat::Range))
	{
		AimPoint = From + Flat.GetSafeNormal2D() * Stats.Get(ECRStat::Range);
	}
	const int32 Tiles = FMath::Clamp(Stats.GetInt(ECRStat::Count), 1, 12);
	const float Radius = Stats.Get(ECRStat::Radius);
	const FVector Step = Direction.GetSafeNormal2D() * Stats.Get(ECRStat::Length);
	UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this);
	TSet<ACRCharacter*> Burned;
	for (int32 i = 0; i < Tiles; ++i)
	{
		// 조준 지점부터 바깥쪽으로 한 칸씩
		FVector Ground;
		TraceGround(GetWorld(), AimPoint + Step * i, Owner, Ground);
		if (Fx)
		{
			Fx->SpawnFlame(Ground, Radius, Tuning.FlameHeight, Tuning.FlameDuration);
		}
		// 불꽃 기둥 가운데 높이에서 반경 판정 + 화상
		TArray<ACRCharacter*> Victims;
		CRCombat::ApplyRadialHit(Owner, ECRWeaponType::Flame, Ground + FVector(0.f, 0.f, Tuning.FlameHeight * 0.35f), Radius + 40.f, -1.f, Form, &Victims);
		for (ACRCharacter* Victim : Victims)
		{
			if (!Burned.Contains(Victim))
			{
				Burned.Add(Victim);
				CRCombat::ApplyBurn(Owner, Victim, Stats.Get(ECRStat::DotDamage), Stats.Get(ECRStat::DotTime));
			}
		}
	}
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

	// 레이저 빔: 원기둥(Z축, 높이 100)을 시작~끝 사이에 늘림. 메시는 필요한 만큼만 만들어 재사용
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	Beams.RemoveAll([](const FBeamFx& Beam) { return Beam.TimeLeft <= 0.f; });
	while (BeamMeshes.Num() < Beams.Num())
	{
		UStaticMeshComponent* Mesh = MakePart(nullptr, CylinderMesh, FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, LaserColor, true, GlowIntensity);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		BeamMeshes.Add(Mesh);
	}
	for (int32 i = 0; i < BeamMeshes.Num(); ++i)
	{
		UStaticMeshComponent* Mesh = BeamMeshes[i];
		if (!Beams.IsValidIndex(i))
		{
			if (Mesh->IsVisible())
			{
				Mesh->SetVisibility(false);
			}
			continue;
		}
		FBeamFx& Beam = Beams[i];
		const float Fade = FMath::Clamp(Beam.TimeLeft / FMath::Max(Tuning.LaserBeamDuration, 0.001f), 0.f, 1.f);
		const FVector Delta = Beam.End - Beam.Start;
		const float Length = Delta.Size();
		const float Width = Beam.Width * Fade / 100.f;
		Mesh->SetWorldLocationAndRotation(Beam.Start + Delta * 0.5f, FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator());
		Mesh->SetWorldScale3D(FVector(Width, Width, Length / 100.f));
		Mesh->SetVisibility(Length > 1.f);
		Beam.TimeLeft -= RealDeltaTime;
	}
}
