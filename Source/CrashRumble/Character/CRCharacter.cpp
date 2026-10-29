#include "Character/CRCharacter.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CRViewComponent.h"
#include "Character/CRCharacterAppearance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTargetSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Feedback/CRFxSubsystem.h"
#include "Feedback/CRScreenFxSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Map/CRBlockArena.h"
#include "Map/CRExplosiveBarrel.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/CRAttributeSet.h"
#include "GAS/CRCombat.h"
#include "GAS/CRGameplayTags.h"
#include "GAS/CRWeaponAbilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Weapon/CRWeaponComponent.h"

namespace
{
	constexpr float RetargetMinInterval = 0.2f;
	constexpr float RetargetMaxInterval = 0.4f;
	constexpr double DpsKeepSeconds = 10.0; // 옵션창 DPS 측정 구간 최대값
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName ColorParam(TEXT("Color"));
	const FName HandLId(TEXT("HandL"));
	const FName HandRId(TEXT("HandR"));

	// 마우스 픽셀 이동량 → 각도
	constexpr float LookScale = 0.15f;

	// 쓰러짐 연출
	constexpr float CapsuleHalfHeight = 100.f;
	constexpr float FallSpeed = 4.f;   // 1/초
	constexpr float ReviveSpeed = 3.f;
	constexpr float FallAngle = 85.f;

	// 몬스터
	const FName AccentTag(TEXT("Accent"));
	constexpr float SkyDropHeight = 1200.f;
	constexpr float FallOutDistance = 3000.f;
	constexpr float MonsterStopDistance = 110.f;

	// 사망 시 벗겨지는 투구(모자) 파츠
	bool IsHelmetPart(FName Id)
	{
		const FString Name = Id.ToString();
		return Name.StartsWith(TEXT("Hat")) || Name.StartsWith(TEXT("Helmet"));
	}
	constexpr float ChunkRestSpeed = 40.f;
	constexpr float ChunkGroundFriction = 3.f;

	// 숨쉬기 / 블롭 그림자
	constexpr float BodyBottomZ = -100.f;     // 캡슐 바닥 (몸통 구가 여기에 닿아 있음)
	constexpr float BodyHeight = 100.f;       // 기본 몸통 지름
	constexpr float HatExtraAmplitude = 1.15f;
	constexpr float WalkBounceFrequency = 2.2f; // 초당 걸음 수
	constexpr float BlobBaseRadius = 50.f;
	constexpr float BlobFadeHeight = 600.f;
	constexpr float BlobMinScale = 0.4f;
	constexpr float BlobTraceDistance = 3000.f;
	constexpr int32 HeroStencil = 1;
	constexpr float HandDiameter = 22.f;

	// 펀치: 왼손이 앞으로, 몸 가운데 쪽으로 뻗는다 (뻗는 거리는 튜닝)
	const FVector PunchSideOffset(0.f, 40.f, 25.f);
	constexpr float PunchStiffness = 180.f;
	constexpr float PunchDamping = 16.f;
	constexpr float SprintFocusMinSpeed = 200.f;

	bool IsHeadPart(const FString& Name)
	{
		return Name.StartsWith(TEXT("Head")) || Name.StartsWith(TEXT("Eye")) || Name.StartsWith(TEXT("Nose"))
			|| Name.StartsWith(TEXT("Mouth")) || Name.StartsWith(TEXT("Face"));
	}
}

ACRCharacter::ACRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(50.f, CapsuleHalfHeight);
	// 몬스터 무리에 카메라가 막혀 밀려 들어오지 않도록
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 스켈레탈 메시는 쓰지 않는다
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetComponentTickEnabled(false);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	Movement->bRunPhysicsWithNoController = true; // 허수아비도 넉백/중력 처리
	Movement->BrakingDecelerationWalking = 4096.f;
	Movement->MaxStepHeight = 55.f; // 블럭 1칸(50cm)은 걸어서 오르내린다

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	ViewComponent = CreateDefaultSubobject<UCRViewComponent>(TEXT("View"));
	WeaponComponent = CreateDefaultSubobject<UCRWeaponComponent>(TEXT("Weapon"));

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<UCRAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* ACRCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ACRCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildParts();
}

void ACRCharacter::BeginPlay()
{
	Super::BeginPlay();

	// PIE 복제 시 Transient 파츠는 따라오지 않으므로 다시 조립
	if (PartComponents.Num() == 0)
	{
		BuildParts();
	}

	HomeLocation = GetActorLocation();
	ViewComponent->Setup(SpringArm, Camera);
	WeaponComponent->Initialize(FindPart(HandRId));
	InitAbilitySystem();

	ApplyTuning();
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		TuningChangedHandle = Tuning->OnChanged.AddUObject(this, &ACRCharacter::ApplyTuning);
	}

	if (IsPlayerControlled())
	{
		WeaponComponent->Equip(ECRWeaponType::Sword);
	}
	RefreshStencil();
}

void ACRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasActorBegunPlay() && IsPlayerControlled() && WeaponComponent->GetEquipped() == ECRWeaponType::None)
	{
		WeaponComponent->Equip(ECRWeaponType::Sword);
	}
	RefreshStencil();
}

void ACRCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
	{
		Tuning->OnChanged.Remove(TuningChangedHandle);
	}
	GetWorldTimerManager().ClearTimer(ReviveTimer);
	Super::EndPlay(EndPlayReason);
}

void ACRCharacter::InitAbilitySystem()
{
	AbilitySystem->InitAbilityActorInfo(this, this);

	if (!bAbilitiesGranted && HasAuthority())
	{
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_SwordSwing::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_LaserFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_ProjectileFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_LightningFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_MissileFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_FlameFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_Punch::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UCRGA_Super::StaticClass()));
		bAbilitiesGranted = true;
	}

	RestoreFullHealth();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UCRAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ACRCharacter::HandleHealthChanged);
}

float ACRCharacter::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ACRCharacter::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ACRCharacter::GetTunedMaxHealth() const
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	return bMonster ? Tuning.MonsterMaxHealth : Tuning.MaxHealth;
}

void ACRCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (TrainingRole == ECRTrainingRole::DpsDummy)
	{
		// DPS 허수아비: 데미지만 기록하고 바로 체력을 채운다 (죽지 않음)
		const float Damage = Data.OldValue - Data.NewValue;
		if (Damage > 0.f)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastDamageTime > UCRTuningSubsystem::GetSettings(this).DpsWindow + 2.f)
			{
				TotalDamage = 0.f; // 한동안 안 맞았으면 새로 측정
				DamageLog.Reset();
			}
			LastDamageTime = Now;
			TotalDamage += Damage;
			DamageLog.Add(TPair<double, float>(Now, Damage));
			// 측정 구간 최대치보다 오래된 기록은 버린다 (계속 맞아도 기록이 끝없이 늘지 않게. 누적 피해는 TotalDamage)
			int32 Expired = 0;
			while (Expired < DamageLog.Num() && Now - DamageLog[Expired].Key > DpsKeepSeconds)
			{
				++Expired;
			}
			if (Expired > 0)
			{
				DamageLog.RemoveAt(0, Expired, EAllowShrinking::No);
			}
			AbilitySystem->SetNumericAttributeBase(UCRAttributeSet::GetHealthAttribute(), GetMaxHealth());
		}
		return;
	}
	if (Data.NewValue <= 0.f && !bDead)
	{
		Die();
	}
}

void ACRCharacter::Die()
{
	bDead = true;
	AbilitySystem->AddLooseGameplayTag(CRTags::State_Dead);
	WeaponComponent->SetTriggerHeld(false);
	WeaponComponent->Equip(WeaponComponent->GetEquipped()); // 휘두르던 중이면 끊는다
	TargetEnemy.Reset();
	UpdatePawnBlocking(); // 시체가 길을 막지 않게

	// 사망 연출은 곧이어 들어오는 넉백(ApplyKnockback) 방향으로 시작한다
	bPendingDeathFx = true;

	if (Team != ECRTeam::None)
	{
		if (UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
		{
			Spawner->NotifyTeamMemberDied(Team);
		}
	}

	// 마지막으로 밀려난 방향(없으면 뒤)으로 넘어진다
	FVector FallDirection = GetVelocity().GetSafeNormal2D();
	if (FallDirection.IsNearlyZero())
	{
		FallDirection = -GetActorForwardVector();
	}
	const FVector LocalFall = GetActorTransform().InverseTransformVectorNoScale(FallDirection);
	FallAxis = FVector::CrossProduct(FVector::UpVector, LocalFall).GetSafeNormal();

	const float Delay = UCRTuningSubsystem::GetSettings(this).RespawnDelay;
	GetWorldTimerManager().SetTimer(ReviveTimer, this, &ACRCharacter::Revive, FMath::Max(Delay, 0.01f), false);
}

void ACRCharacter::Revive()
{
	if (bMonster)
	{
		// 몬스터는 쓰러진 자리에서 일어나지 않고 하늘에서 새로 떨어진다
		Respawn();
		ResetPose();
	}
	else
	{
		Reassemble(); // 벗겨진 투구/무기를 제자리로 (몸은 천천히 일어난다)
		if (TrainingRole != ECRTrainingRole::None)
		{
			// 훈련장 허수아비는 처음 자리로 돌아와 일어난다
			Respawn();
		}
	}
	RestoreFullHealth();
	AbilitySystem->RemoveLooseGameplayTag(CRTags::State_Dead);
	bDead = false;
	UpdatePawnBlocking();
}

void ACRCharacter::UpdatePawnBlocking()
{
	const bool bBlock = !bDead && !bSpectating;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bBlock ? ECR_Block : ECR_Ignore);
}

void ACRCharacter::StartDeathFx(const FVector& HitVelocity, bool bLaunchBody)
{
	bPendingDeathFx = false;
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 몸: 맞은 방향으로 더 세게 밀려나며 그쪽으로 넘어진다
	FVector Dir = HitVelocity.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector();
	}
	if (bLaunchBody)
	{
		LaunchCharacter(HitVelocity * Tuning.DeathKnockbackScale, false, true);
	}
	const FVector LocalFall = GetActorTransform().InverseTransformVectorNoScale(Dir);
	FallAxis = FVector::CrossProduct(FVector::UpVector, LocalFall).GetSafeNormal();

	if (!Tuning.bDeathPopOff || bPoppedOff)
	{
		return;
	}

	// 바닥 높이 (공중에서 죽었을 수도 있으니 한 번 쏴서 찾는다. 바닥이 WorldDynamic일 수 있음)
	DeathGroundZ = GetActorLocation().Z - CapsuleHalfHeight;
	FCollisionObjectQueryParams GroundTypes;
	GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CRDeathGround), false, this);
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByObjectType(GroundHit, GetActorLocation(), GetActorLocation() - FVector(0, 0, 3000.f), GroundTypes, Params))
	{
		DeathGroundZ = GroundHit.ImpactPoint.Z;
	}

	DeathChunks.Reset();

	// 투구(모자): 위로 튀어 오르며 돈다
	FDeathChunk Helmet;
	for (int32 i = 0; i < PartComponents.Num() && i < PartRestTransforms.Num(); ++i)
	{
		UStaticMeshComponent* Part = PartComponents[i];
		if (Part && Part->ComponentTags.Num() > 0 && IsHelmetPart(Part->ComponentTags[0]))
		{
			Helmet.Components.Add(Part);
			Helmet.RestTransforms.Add(PartRestTransforms[i]);
		}
	}
	if (Helmet.Components.Num() > 0)
	{
		const FVector Side = FMath::VRand().GetSafeNormal2D();
		Helmet.Velocity = (Dir * 0.6f + Side * 0.4f) * Tuning.DeathPopSpeed + FVector::UpVector * Tuning.DeathPopUp;
		DeathChunks.Add(MoveTemp(Helmet));
	}

	// 무기: 손에서 빠져 옆으로 날아간다
	USceneComponent* WeaponRoot = WeaponComponent->GetWeaponRoot();
	if (WeaponRoot && WeaponComponent->GetEquipped() != ECRWeaponType::None)
	{
		FDeathChunk Weapon;
		Weapon.Components.Add(WeaponRoot);
		Weapon.RestTransforms.Add(FTransform::Identity);
		const FVector Throw = Dir.RotateAngleAxis(FMath::FRandRange(-70.f, 70.f), FVector::UpVector);
		Weapon.Velocity = Throw * Tuning.DeathPopSpeed + FVector::UpVector * Tuning.DeathPopUp * 0.8f;
		DeathChunks.Add(MoveTemp(Weapon));
	}

	const float Spin = FMath::DegreesToRadians(Tuning.DeathSpin);
	for (FDeathChunk& Chunk : DeathChunks)
	{
		// 조각 기준점 = 보이는 메시들의 경계 상자 중심
		FBox Box(ForceInit);
		for (const TWeakObjectPtr<USceneComponent>& Component : Chunk.Components)
		{
			TArray<USceneComponent*> Attached;
			Component->GetChildrenComponents(true, Attached);
			Attached.Add(Component.Get());
			for (const USceneComponent* Child : Attached)
			{
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
				if (Primitive && Primitive->IsVisible())
				{
					Box += Primitive->Bounds.GetBox();
				}
			}
		}
		const FTransform Origin(GetActorQuat(), Box.IsValid ? Box.GetCenter() : Chunk.Components[0]->GetComponentLocation());
		Chunk.Location = Origin.GetLocation();
		Chunk.Rotation = Origin.GetRotation();
		Chunk.Radius = Box.IsValid ? FMath::Max(Box.GetExtent().GetMin(), 4.f) : 10.f;
		for (const TWeakObjectPtr<USceneComponent>& Component : Chunk.Components)
		{
			Chunk.Offsets.Add(Component->GetComponentTransform().GetRelativeTransform(Origin));
		}
		Chunk.SpinAxis = FMath::VRand();
		Chunk.SpinSpeed = Spin * FMath::FRandRange(0.7f, 1.3f);
	}
	bPoppedOff = DeathChunks.Num() > 0;
}

void ACRCharacter::UpdatePopOff(float DeltaSeconds)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	for (FDeathChunk& Chunk : DeathChunks)
	{
		if (!Chunk.bResting)
		{
			Chunk.Velocity.Z -= Tuning.DeathGravity * DeltaSeconds;
			Chunk.Location += Chunk.Velocity * DeltaSeconds;
			Chunk.Rotation = FQuat(Chunk.SpinAxis, Chunk.SpinSpeed * DeltaSeconds) * Chunk.Rotation;

			const float Floor = DeathGroundZ + Chunk.Radius;
			if (Chunk.Location.Z <= Floor)
			{
				Chunk.Location.Z = Floor;
				if (Chunk.Velocity.Z < 0.f)
				{
					// 튕길 때마다 수평 속도와 회전이 줄어든다
					Chunk.Velocity.Z = -Chunk.Velocity.Z * Tuning.DeathBounce;
					Chunk.Velocity.X *= 0.7f;
					Chunk.Velocity.Y *= 0.7f;
					Chunk.SpinSpeed *= 0.6f;
				}
				if (Chunk.Velocity.Z < ChunkRestSpeed * 2.f)
				{
					// 바닥에 붙어 미끄러지다 멈춘다
					Chunk.Velocity.Z = 0.f;
					const float Friction = FMath::Max(0.f, 1.f - ChunkGroundFriction * DeltaSeconds);
					Chunk.Velocity.X *= Friction;
					Chunk.Velocity.Y *= Friction;
					Chunk.SpinSpeed *= Friction;
					Chunk.bResting = Chunk.Velocity.Size2D() < ChunkRestSpeed;
				}
			}
		}

		const FTransform ChunkTransform(Chunk.Rotation, Chunk.Location);
		for (int32 i = 0; i < Chunk.Components.Num(); ++i)
		{
			if (USceneComponent* Component = Chunk.Components[i].Get())
			{
				Component->SetWorldTransform(Chunk.Offsets[i] * ChunkTransform);
			}
		}
	}
}

void ACRCharacter::Reassemble()
{
	bPendingDeathFx = false;
	if (!bPoppedOff)
	{
		return;
	}
	bPoppedOff = false;
	for (const FDeathChunk& Chunk : DeathChunks)
	{
		for (int32 i = 0; i < Chunk.Components.Num(); ++i)
		{
			if (USceneComponent* Component = Chunk.Components[i].Get())
			{
				Component->SetRelativeTransform(Chunk.RestTransforms[i]);
			}
		}
	}
	DeathChunks.Reset();
}

void ACRCharacter::RestoreFullHealth()
{
	const float MaxHealth = GetTunedMaxHealth();
	AbilitySystem->SetNumericAttributeBase(UCRAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(UCRAttributeSet::GetHealthAttribute(), MaxHealth);
}

void ACRCharacter::ResetPose()
{
	Reassemble();
	FallAlpha = 0.f;
	VisualRoot->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
}

void ACRCharacter::Respawn()
{
	FVector Location = HomeLocation;
	if (bMonster)
	{
		if (const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
		{
			Spawner->FindRespawnLocation(Team, SkyDropHeight, Location);
		}
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}

void ACRCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// 엔진 기본 동작(액터 삭제) 대신 다시 등장
	Respawn();
}

void ACRCharacter::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_CharacterTick);
	RestoreCharStepping();
	Super::Tick(DeltaSeconds);
	if (bInPool)
	{
		return;
	}

	// 바닥 밖으로 떨어지면 KillZ까지 기다리지 않고 바로 재등장
	if (GetActorLocation().Z < HomeLocation.Z - FallOutDistance)
	{
		Respawn();
	}

	// 넉백 없이 죽은 경우 (넉백 끔 등): 지금 속도 방향으로
	if (bPendingDeathFx)
	{
		StartDeathFx(GetVelocity(), false);
	}

	UpdateChase(DeltaSeconds);
	UpdatePatrol(DeltaSeconds);
	UpdatePunch(DeltaSeconds);
	// 캐릭터 LOD: 멀수록 파츠 움직임(숨쉬기, 손, 무기 자세)을 덜 갱신. 몸 이동/회전/넘어짐은 그대로
	VisualLod = ComputeVisualLod();
	LodPendingDelta += DeltaSeconds;
	const UCRTuningSettings& LodTuning = UCRTuningSubsystem::GetSettings(this);
	const int32 MidInterval = FMath::Max(LodTuning.CharLodMidInterval, 1);
	if (VisualLod == 0 || (VisualLod == 1 && (GFrameCounter + GetUniqueID()) % uint64(MidInterval) == 0))
	{
		UpdateBreath(LodPendingDelta);
		UpdateHands(LodPendingDelta);
		LodPendingDelta = 0.f;
	}
	else if (VisualLod == 2)
	{
		LodPendingDelta = 0.f;
	}
	const int32 DebugLod = LodTuning.bCharLodDebug ? VisualLod : -1;
	if (DebugLod != LodDebugShown)
	{
		static const FLinearColor LodColors[] = { FLinearColor(0.1f, 0.9f, 0.2f), FLinearColor(1.f, 0.85f, 0.1f), FLinearColor(1.f, 0.1f, 0.1f) };
		SetAccentPartsColor(DebugLod >= 0 ? LodColors[DebugLod] : AccentColor);
		LodDebugShown = DebugLod;
	}
	UpdateFacing(DeltaSeconds);
	UpdateFlash(DeltaSeconds);
	UpdateFall(DeltaSeconds);
	if (bPoppedOff)
	{
		UpdatePopOff(DeltaSeconds); // 몸 기울기 뒤에 월드 좌표로 덮어쓴다
	}
	if (VisualLod == 0)
	{
		UpdateSmear(DeltaSeconds);
	}
	UpdateBlobShadow();
	ApplyCharStepping();
}

int32 ACRCharacter::ComputeVisualLod() const
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (!Tuning.bCharLod || IsPlayerControlled())
	{
		return 0;
	}
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	if (!Viewer || !Viewer->PlayerCameraManager)
	{
		return 0;
	}
	const float DistSq = FVector::DistSquared(Viewer->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
	return DistSq < FMath::Square(Tuning.CharLodNear) ? 0 : DistSq < FMath::Square(Tuning.CharLodFar) ? 1 : 2;
}

void ACRCharacter::Landed(const FHitResult& Hit)
{
	// 착지 순간에는 아직 떨어지던 속도가 남아 있다
	const float LandingSpeed = -GetCharacterMovement()->Velocity.Z;
	Super::Landed(Hit);

	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (Tuning.bDustRing && LandingSpeed >= Tuning.DustRingMinSpeed && !IsHidden())
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			const FVector Feet(GetActorLocation().X, GetActorLocation().Y, Hit.ImpactPoint.Z);
			Fx->SpawnDustRing(Feet, FMath::Clamp(LandingSpeed / 900.f, 0.6f, 1.6f));
		}
	}
}

void ACRCharacter::UpdateBreath(float DeltaSeconds)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	BreathTime = FMath::Fmod(BreathTime + DeltaSeconds * Tuning.BreathSpeed * UE_TWO_PI, UE_TWO_PI * 100.f);

	// 걸을 때만 통통 (속도에 비례해 서서히 켜짐)
	const bool bWalking = GetCharacterMovement()->IsMovingOnGround() && !bDead;
	const float TargetWalk = bWalking ? FMath::Clamp(GetVelocity().Size2D() / 300.f, 0.f, 1.f) : 0.f;
	WalkAlpha = FMath::FInterpTo(WalkAlpha, TargetWalk, DeltaSeconds, 8.f);
	WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * WalkBounceFrequency * UE_PI * WalkAlpha, UE_PI * 100.f);
	const float Bounce = FMath::Abs(FMath::Sin(WalkPhase)) * Tuning.BreathMoveBounce * WalkAlpha;

	const float Amplitude = Tuning.bBreathing && !bDead ? Tuning.BreathAmplitude : 0.f;
	const float Stretch = Amplitude * FMath::Sin(BreathTime);
	const float HeadRise = BodyHeight * Amplitude * FMath::Sin(BreathTime - Tuning.BreathHeadLag);
	const float HatRise = BodyHeight * Amplitude * HatExtraAmplitude * FMath::Sin(BreathTime - Tuning.BreathHeadLag - Tuning.BreathHatLag);

	for (int32 i = 0; i < PartComponents.Num() && i < PartRestTransforms.Num() && i < PartBreathGroups.Num(); ++i)
	{
		UStaticMeshComponent* Part = PartComponents[i];
		const EBreathGroup Group = PartBreathGroups[i];
		if (!Part || Group == EBreathGroup::None)
		{
			continue;
		}
		const FTransform& Rest = PartRestTransforms[i];
		FVector Location = Rest.GetLocation();
		FVector Scale = Rest.GetScale3D();
		switch (Group)
		{
		case EBreathGroup::Body:
			// 바닥은 붙인 채 위로 늘어나고 옆으로 살짝 줄어든다
			Location.Z = BodyBottomZ + (Location.Z - BodyBottomZ) * (1.f + Stretch);
			Scale *= FVector(1.f - Stretch * 0.5f, 1.f - Stretch * 0.5f, 1.f + Stretch);
			break;
		case EBreathGroup::OnBody:
			Location.Z = BodyBottomZ + (Location.Z - BodyBottomZ) * (1.f + Stretch);
			Location.X *= 1.f - Stretch * 0.5f;
			Location.Y *= 1.f - Stretch * 0.5f;
			break;
		case EBreathGroup::Head:
			Location.Z += HeadRise;
			break;
		case EBreathGroup::Hat:
			Location.Z += HatRise;
			break;
		default:
			break;
		}
		Location.Z += Bounce;
		Part->SetRelativeTransform(FTransform(Rest.GetRotation(), Location, Scale));
	}
}

void ACRCharacter::UpdateBlobShadow()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UCRFxSubsystem* Fx = Tuning.bBlobShadow ? UCRFxSubsystem::Get(this) : nullptr;
	if (!Fx || IsHidden())
	{
		return;
	}

	// 걷는 중이면 이동 컴포넌트가 이미 찾은 바닥, 공중이면 한 번 쏜다
	const FVector Location = GetActorLocation();
	float GroundZ = 0.f;
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->IsMovingOnGround() && Movement->CurrentFloor.bBlockingHit)
	{
		GroundZ = Movement->CurrentFloor.HitResult.ImpactPoint.Z;
	}
	else
	{
		FCollisionObjectQueryParams GroundTypes;
		GroundTypes.AddObjectTypesToQuery(ECC_WorldStatic);
		GroundTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CRBlobShadow), false, this);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0.f, 0.f, BlobTraceDistance), GroundTypes, Params))
		{
			return;
		}
		GroundZ = Hit.ImpactPoint.Z;
	}

	// 공중에 있을수록 작고 옅게 (최소 40%)
	const float Height = FMath::Max(0.f, Location.Z + BodyBottomZ - GroundZ);
	const float Scale = FMath::Max(BlobMinScale, 1.f - Height / BlobFadeHeight);
	Fx->AddBlobShadow(FVector(Location.X, Location.Y, GroundZ), BlobBaseRadius * Tuning.BlobShadowSize * Scale,
		Tuning.BlobShadowStrength * Scale);
}

void ACRCharacter::UpdateSmear(float DeltaSeconds)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this);
	if (!Fx || !Tuning.bSmear || IsHidden())
	{
		SmearHandLocations.Reset();
		return;
	}
	SmearElapsed += DeltaSeconds;
	// 그림이 바뀌는 프레임에만 비교한다 (지난 그림 ↔ 이번 그림)
	if (!Fx->IsFxDrawFrame() || Fx->GetFxDrawCount() == SmearDrawCount)
	{
		return;
	}
	SmearDrawCount = Fx->GetFxDrawCount();
	const float Elapsed = FMath::Max(SmearElapsed, 0.001f);
	SmearElapsed = 0.f;

	const FTransform ActorTransform = GetActorTransform();
	for (const TPair<FName, FVector>& Rest : HandRestLocations)
	{
		const UStaticMeshComponent* Hand = FindPart(Rest.Key);
		if (!Hand)
		{
			continue;
		}
		// 몸 기준 위치: 달리기만 해서는 늘어나지 않는다
		const FVector Local = ActorTransform.InverseTransformPosition(Hand->GetComponentLocation());
		if (const FVector* Previous = SmearHandLocations.Find(Rest.Key))
		{
			const float Distance = FVector::Dist(*Previous, Local);
			if (Distance / Elapsed >= Tuning.SmearMinSpeed && Distance > HandDiameter)
			{
				Fx->SpawnSmear(ActorTransform.TransformPosition(*Previous), Hand->GetComponentLocation(), HandDiameter, AccentColor);
			}
		}
		SmearHandLocations.Add(Rest.Key, Local);
	}
}

void ACRCharacter::RestoreCharStepping()
{
	if (bStepApplied)
	{
		// 모션 계산은 원래 위치에서 이어간다
		VisualRoot->SetRelativeTransform(StepRootMotion);
		bStepApplied = false;
	}
}

void ACRCharacter::ApplyCharStepping()
{
	const UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this);
	if (!Fx || !Fx->IsCharSteppingEnabled())
	{
		bHasStepSnapshot = false;
		return;
	}
	if (Fx->IsCharDrawFrame() || !bHasStepSnapshot || StepPartTransforms.Num() != PartComponents.Num())
	{
		// 이번 장을 기억 (루트는 월드 좌표, 파츠는 상대 좌표)
		StepRootWorld = VisualRoot->GetComponentTransform();
		StepPartTransforms.SetNum(PartComponents.Num());
		for (int32 i = 0; i < PartComponents.Num(); ++i)
		{
			if (PartComponents[i])
			{
				StepPartTransforms[i] = PartComponents[i]->GetRelativeTransform();
			}
		}
		bHasStepSnapshot = true;
		return;
	}
	// 그리지 않는 프레임: 화면에는 마지막 장을 그대로 둔다
	StepRootMotion = VisualRoot->GetRelativeTransform();
	VisualRoot->SetWorldTransform(StepRootWorld);
	for (int32 i = 0; i < PartComponents.Num(); ++i)
	{
		if (PartComponents[i])
		{
			PartComponents[i]->SetRelativeTransform(StepPartTransforms[i]);
		}
	}
	bStepApplied = true;
}

void ACRCharacter::RefreshSurfaceMaterial()
{
	if (!bUseCustomPrimitiveData)
	{
		return;
	}
	// 외형 에셋이 머티리얼을 지정했으면 그대로 둔다
	if (Appearance && !Appearance->Material.IsNull())
	{
		return;
	}
	UMaterialInterface* Material = UCRGameSettings::LoadSurfaceMaterial(this);
	const bool bToon = UCRTuningSubsystem::GetSettings(this).bToonShading;
	if (Material && Material != SurfaceMaterial)
	{
		SurfaceMaterial = Material;
		for (UStaticMeshComponent* Part : PartComponents)
		{
			if (Part)
			{
				Part->SetMaterial(0, Material);
				// Unlit 표면은 그림자를 받지 않으니 캐스팅도 끈다 (블롭 그림자가 대신함)
				Part->SetCastShadow(!bToon);
			}
		}
	}
	WeaponComponent->RefreshSurfaceMaterial();
}

void ACRCharacter::RefreshStencil()
{
	const bool bHero = IsPlayerControlled();
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive != GetCapsuleComponent() && Primitive != GetMesh())
		{
			Primitive->SetRenderCustomDepth(bHero);
			Primitive->SetCustomDepthStencilValue(bHero ? HeroStencil : 0);
		}
	}
}

void ACRCharacter::UpdateChase(float DeltaSeconds)
{
	if (!bMonster || bDead || TrainingRole != ECRTrainingRole::None)
	{
		return;
	}
	if (Team != ECRTeam::None)
	{
		UpdateBattle(DeltaSeconds);
		return;
	}
	const float Speed = UCRTuningSubsystem::GetSettings(this).MonsterMoveSpeed;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Speed <= 0.f || !Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.f;
	const float Distance = ToPlayer.Size();
	if (Distance < 1.f)
	{
		return;
	}
	const FVector Direction = ToPlayer / Distance;
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Direction.Rotation().Yaw, 0.f), DeltaSeconds, 8.f));
	if (Distance > MonsterStopDistance)
	{
		AddMovementInput(Direction, FMath::Clamp(Speed / GetCharacterMovement()->MaxWalkSpeed, 0.f, 1.f));
	}
}

void ACRCharacter::UpdateBattle(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_UpdateBattle);
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this);

	// 가장 가까운 적은 가끔만 다시 찾는다 (다 같이 같은 프레임에 찾지 않도록 간격을 흩뜨림)
	RetargetTimeLeft -= DeltaSeconds;
	ACRCharacter* Enemy = TargetEnemy.Get();
	// 표적이 죽거나 풀로 돌아가면 바로, 그 외(표적 없음 포함)엔 타이머마다만 찾는다 (생성 직후 한꺼번에 몰리지 않게)
	if (RetargetTimeLeft <= 0.f || (Enemy && (Enemy->IsDead() || Enemy->IsInPool())))
	{
		Enemy = Spawner ? Spawner->FindNearestEnemy(this) : nullptr;
		TargetEnemy = Enemy;
		RetargetTimeLeft = FMath::FRandRange(RetargetMinInterval, RetargetMaxInterval);
	}
	// 줄 서기 전투: 교전 거리 안에 적이 없으면 자기 줄을 지키며 앞으로 전진 (상대 출발선을 넘으면 그냥 가장 가까운 적에게)
	const FCRBattleSetup* Setup = Spawner ? &Spawner->GetBattleSetup() : nullptr;
	if (Setup && Setup->bLineFormation
		&& (!Enemy || FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation()) > FMath::Square(Setup->EngageRange))
		&& Spawner->GetAdvance(Team, GetActorLocation()) < Setup->TeamDistance * 2.f)
	{
		WeaponComponent->SetTriggerHeld(false);
		const FVector Forward = Spawner->GetTeamForward(Team);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Forward.Rotation().Yaw, 0.f), DeltaSeconds, 10.f));
		AddMovementInput(Forward, FMath::Clamp(Tuning.BattleMoveSpeed / GetCharacterMovement()->MaxWalkSpeed, 0.f, 1.f));
		return;
	}
	if (!Enemy)
	{
		WeaponComponent->SetTriggerHeld(false);
		return;
	}

	FVector ToEnemy = Enemy->GetActorLocation() - GetActorLocation();
	ToEnemy.Z = 0.f;
	const float Distance = ToEnemy.Size();
	const FVector Direction = Distance > 1.f ? ToEnemy / Distance : GetActorForwardVector();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Direction.Rotation().Yaw, 0.f), DeltaSeconds, 10.f));

	// 사거리 안이면 공격(간격은 무기 쪽에서), 너무 붙지 않게 조금 앞에서 멈춘다.
	// 총은 거의 정면을 볼 때만 쏜다 (돌아서는 중에 엉뚱한 곳으로 쏘지 않게)
	const bool bGun = BattleWeapon != ECRWeaponType::Sword;
	const float Range = bGun ? Tuning.BattleGunRange : Tuning.BattleAttackRange;
	const bool bFacing = FVector::DotProduct(GetActorForwardVector(), Direction) > 0.9f;
	WeaponComponent->SetTriggerHeld(Distance <= Range && (!bGun || bFacing));
	const float MoveScale = FMath::Clamp(Tuning.BattleMoveSpeed / GetCharacterMovement()->MaxWalkSpeed, 0.f, 1.f);
	if (Distance > Range * 0.8f)
	{
		AddMovementInput(Direction, MoveScale);
	}
	else if (const float Strafe = Spawner ? Spawner->GetBattleSetup().Strafe : 0.f; Strafe > 0.f)
	{
		// 사거리 안: 적을 본 채로 좌우로 옆걸음 (가끔 방향을 바꾼다)
		StrafeTimeLeft -= DeltaSeconds;
		if (StrafeTimeLeft <= 0.f)
		{
			StrafeSign = FMath::RandBool() ? 1.f : -1.f;
			StrafeTimeLeft = FMath::FRandRange(0.8f, 2.f);
		}
		AddMovementInput(FVector::CrossProduct(FVector::UpVector, Direction) * StrafeSign, MoveScale * Strafe);
	}
}

void ACRCharacter::SetupAsMonster(const FLinearColor& InAccentColor, ECRTeam InTeam, ECRWeaponType InWeapon)
{
	bMonster = true;
	Team = InTeam;
	TrainingRole = ECRTrainingRole::None;
	BattleWeapon = InWeapon == ECRWeaponType::None ? ECRWeaponType::Sword : InWeapon;
	TargetEnemy.Reset();
	RetargetTimeLeft = FMath::FRandRange(0.f, RetargetMaxInterval);
	SetAccentColor(InAccentColor);

	// 몬스터에겐 카메라가 필요 없다 (수백 마리일 때 틱 비용 절약). 무기는 팀 전투일 때만.
	SpringArm->SetComponentTickEnabled(false);
	ViewComponent->SetComponentTickEnabled(false);
	const bool bArmed = Team != ECRTeam::None;
	WeaponComponent->SetTriggerHeld(false);
	WeaponComponent->Equip(bArmed ? BattleWeapon : ECRWeaponType::None);
	WeaponComponent->SetComponentTickEnabled(bArmed);
	ApplyTuning();
	RestoreFullHealth();
}

void ACRCharacter::SetSpectating(bool bInSpectating)
{
	bSpectating = bInSpectating;
	SetActorHiddenInGame(bSpectating);
	UpdatePawnBlocking();
	WeaponComponent->SetTriggerHeld(false);
}

void ACRCharacter::SetAccentColor(const FLinearColor& InAccentColor)
{
	AccentColor = InAccentColor;
	LodDebugShown = -1;
	SetAccentPartsColor(AccentColor);
}

void ACRCharacter::SetAccentPartsColor(const FLinearColor& Color)
{
	for (UStaticMeshComponent* Part : PartComponents)
	{
		if (Part && Part->ComponentTags.Contains(AccentTag))
		{
			if (bUseCustomPrimitiveData)
			{
				Part->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, 0.f));
			}
			else if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
			{
				Material->SetVectorParameterValue(ColorParam, Color);
			}
		}
	}
}

void ACRCharacter::OnPoolAcquired()
{
	bInPool = false;
	HomeLocation = GetActorLocation();
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	GetCharacterMovement()->SetComponentTickEnabled(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	GetWorldTimerManager().ClearTimer(ReviveTimer);
	if (AbilitySystem->AbilityActorInfo.IsValid())
	{
		RestoreFullHealth();
		AbilitySystem->RemoveLooseGameplayTag(CRTags::State_Dead, AbilitySystem->GetTagCount(CRTags::State_Dead));
	}
	bDead = false;
	ResetPose();
	UpdatePawnBlocking();
	FlashTimeLeft = 0.f;
	SetPartsFlash(0.f);
}

void ACRCharacter::OnPoolReleased()
{
	bInPool = true;
	Team = ECRTeam::None;
	TargetEnemy.Reset();
	WeaponComponent->SetTriggerHeld(false);
	GetWorldTimerManager().ClearTimer(ReviveTimer);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
}

void ACRCharacter::MoveInput(const FVector2D& Axis)
{
	if (bDead)
	{
		return;
	}
	const FRotator YawRotation(0.f, ViewComponent->GetMovementYaw(), 0.f);
	const FRotationMatrix Matrix(YawRotation);
	AddMovementInput(Matrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Matrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ACRCharacter::LookInput(const FVector2D& Axis)
{
	if (ViewComponent->GetViewMode() != ECRViewMode::Shoulder)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const float Scale = LookScale * Tuning.MouseSensitivity;
	AddControllerYawInput(Axis.X * Scale);
	// 엔진 피치 입력은 위가 음수라 마우스 위(+Y)를 뒤집어 넘긴다
	AddControllerPitchInput(-Axis.Y * Scale * (Tuning.bInvertMouseY ? -1.f : 1.f));
}

void ACRCharacter::ToggleViewMode()
{
	ViewComponent->ToggleViewMode();
}

UStaticMeshComponent* ACRCharacter::FindPart(FName Id) const
{
	for (UStaticMeshComponent* Part : PartComponents)
	{
		if (Part && Part->ComponentTags.Num() > 0 && Part->ComponentTags[0] == Id)
		{
			return Part;
		}
	}
	return nullptr;
}

void ACRCharacter::BuildParts()
{
	// 이전에 만든 파츠(목록에서 빠진 복제본 포함) 정리
	TArray<UStaticMeshComponent*> Existing;
	GetComponents<UStaticMeshComponent>(Existing);
	for (UStaticMeshComponent* Part : Existing)
	{
		if (Part->HasAnyFlags(RF_Transient) && Part->ComponentTags.Num() > 0)
		{
			Part->DestroyComponent();
		}
	}
	PartComponents.Reset();
	PartRestTransforms.Reset();
	PartBreathGroups.Reset();
	HandRestLocations.Reset();
	StepPartTransforms.Reset();
	bHasStepSnapshot = false;

	const TArray<FCRCharacterPartSpec>& Specs = Appearance ? Appearance->Parts : UCRCharacterAppearance::MakeDefaultParts();

	// 우선순위: 외형 에셋 머티리얼 > 프로젝트 설정 캐릭터 머티리얼(CPD) > 엔진 기본(MID)
	UMaterialInterface* BaseMaterial = Appearance && !Appearance->Material.IsNull() ? Appearance->Material.LoadSynchronous() : nullptr;
	if (!BaseMaterial)
	{
		BaseMaterial = UCRGameSettings::LoadSurfaceMaterial(this);
	}
	bUseCustomPrimitiveData = BaseMaterial != nullptr;
	SurfaceMaterial = BaseMaterial;
	const bool bToon = UCRTuningSubsystem::GetSettings(this).bToonShading;
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
	}

	for (const FCRCharacterPartSpec& Spec : Specs)
	{
		UStaticMesh* PartMesh = Spec.Mesh.LoadSynchronous();
		if (!PartMesh)
		{
			continue;
		}

		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetStaticMesh(PartMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->ComponentTags.Add(Spec.Id);
		if (Spec.bUseAccentColor)
		{
			Part->ComponentTags.Add(AccentTag);
		}
		Part->SetupAttachment(VisualRoot);
		Part->SetRelativeTransform(Spec.Transform);
		Part->SetCastShadow(!bToon);
		Part->RegisterComponent();

		const FLinearColor Color = Spec.bUseAccentColor ? AccentColor : Spec.Color;
		if (bUseCustomPrimitiveData)
		{
			Part->SetMaterial(0, BaseMaterial);
			Part->SetCustomPrimitiveDataVector4(CRMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, 0.f));
			Part->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, 0.f);
		}
		else if (BaseMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Part);
			Material->SetVectorParameterValue(ColorParam, Color);
			Part->SetMaterial(0, Material);
		}

		const FString IdName = Spec.Id.ToString();
		EBreathGroup Group = EBreathGroup::None;
		if (Spec.Id == HandLId || Spec.Id == HandRId)
		{
			HandRestLocations.Add(Spec.Id, Spec.Transform.GetLocation());
		}
		else if (IsHelmetPart(Spec.Id))
		{
			Group = EBreathGroup::Hat;
		}
		else if (IsHeadPart(IdName))
		{
			Group = EBreathGroup::Head;
		}
		else if (IdName.StartsWith(TEXT("Body")))
		{
			Group = EBreathGroup::Body;
		}
		else
		{
			Group = EBreathGroup::OnBody;
		}
		PartComponents.Add(Part);
		PartRestTransforms.Add(Spec.Transform);
		PartBreathGroups.Add(Group);
	}
}

void ACRCharacter::ApplyTuning()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UpdateMoveSpeed();
	GetCharacterMovement()->MaxAcceleration = Tuning.MaxAcceleration;
	RefreshSurfaceMaterial();

	// 최대 체력이 바뀌면 비율 유지
	if (AbilitySystem && AbilitySystem->AbilityActorInfo.IsValid() && !bDead)
	{
		const float OldMax = GetMaxHealth();
		const float NewMax = GetTunedMaxHealth();
		if (!FMath::IsNearlyEqual(OldMax, NewMax) && OldMax > 0.f)
		{
			const float Ratio = GetHealth() / OldMax;
			AbilitySystem->SetNumericAttributeBase(UCRAttributeSet::GetMaxHealthAttribute(), NewMax);
			AbilitySystem->SetNumericAttributeBase(UCRAttributeSet::GetHealthAttribute(), FMath::Max(1.f, NewMax * Ratio));
		}
	}
}

void ACRCharacter::SetupTraining(ECRTrainingRole InRole, const FVector& PatrolA, const FVector& PatrolB, float Speed)
{
	TrainingRole = InRole;
	PatrolPoints[0] = PatrolA;
	PatrolPoints[1] = PatrolB;
	PatrolIndex = 1;
	PatrolSpeed = Speed;
	HomeLocation = GetActorLocation();
	// 역할별 모자 색: 허수아비 주황, DPS 노랑, 움직이는 표적 보라
	const FLinearColor Color = InRole == ECRTrainingRole::DpsDummy ? FLinearColor(1.f, 0.8f, 0.1f)
		: InRole == ECRTrainingRole::MovingTarget ? FLinearColor(0.55f, 0.25f, 1.f) : FLinearColor(1.f, 0.35f, 0.1f);
	SetAccentColor(Color);
	if (InRole == ECRTrainingRole::MovingTarget)
	{
		GetCharacterMovement()->MaxWalkSpeed = FMath::Max(Speed, 50.f);
	}
}

float ACRCharacter::GetDps() const
{
	const double Now = FPlatformTime::Seconds();
	const float Window = FMath::Max(UCRTuningSubsystem::GetSettings(this).DpsWindow, 0.1f);
	float Sum = 0.f;
	double First = Now;
	for (const TPair<double, float>& Entry : DamageLog)
	{
		if (Now - Entry.Key <= Window)
		{
			Sum += Entry.Value;
			First = FMath::Min(First, Entry.Key);
		}
	}
	// 측정을 막 시작했으면 지난 시간으로 나눈다 (첫 타에 숫자가 튀지 않게 최소 0.5초)
	return Sum / FMath::Clamp(float(Now - First), 0.5f, Window);
}

void ACRCharacter::UpdatePatrol(float DeltaSeconds)
{
	if (TrainingRole != ECRTrainingRole::MovingTarget || bDead)
	{
		return;
	}
	FVector ToGoal = PatrolPoints[PatrolIndex] - GetActorLocation();
	ToGoal.Z = 0.f;
	if (ToGoal.Size() < 30.f)
	{
		PatrolIndex = 1 - PatrolIndex;
		return;
	}
	AddMovementInput(ToGoal.GetSafeNormal(), 1.f);
}

void ACRCharacter::UpdateMoveSpeed()
{
	if (TrainingRole == ECRTrainingRole::MovingTarget)
	{
		GetCharacterMovement()->MaxWalkSpeed = FMath::Max(PatrolSpeed, 50.f);
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	// 몬스터는 이동 입력 크기로 속도를 줄이므로 최대 속도는 둘 중 큰 값
	GetCharacterMovement()->MaxWalkSpeed = bMonster
		? FMath::Max3(Tuning.MoveSpeed, Tuning.MonsterMoveSpeed, Tuning.BattleMoveSpeed)
		: Tuning.MoveSpeed * (bSprinting ? Tuning.SprintMultiplier : 1.f);
}

void ACRCharacter::SetSprinting(bool bInSprinting)
{
	bSprinting = bInSprinting;
	UpdateMoveSpeed();
}

void ACRCharacter::TryPunch()
{
	if (bDead || PunchClock < NextPunchTime)
	{
		return;
	}
	if (AbilitySystem->TryActivateAbilityByClass(UCRGA_Punch::StaticClass()))
	{
		NextPunchTime = PunchClock + UCRTuningSubsystem::GetSettings(this).PunchInterval;
	}
}

void ACRCharacter::TrySuper()
{
	const double Now = FPlatformTime::Seconds();
	if (bDead || Now < NextSuperTime)
	{
		return;
	}
	if (AbilitySystem->TryActivateAbilityByClass(UCRGA_Super::StaticClass()))
	{
		const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
		NextSuperTime = Now + Tuning.SuperCooldown + Tuning.SuperFreezeDuration;
	}
}

void ACRCharacter::PerformPunch()
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);

	// 커서 쪽으로 즉시 회전
	FVector AimPoint;
	if (GetWeaponAimPoint(AimPoint))
	{
		const FVector ToAim = AimPoint - GetActorLocation();
		if (ToAim.SizeSquared2D() > 1.f)
		{
			SetActorRotation(FRotator(0.f, ToAim.Rotation().Yaw, 0.f));
		}
	}

	// 왼손을 한 프레임에 뻗는다 (→ 스미어)
	PunchExtend = 1.f;
	PunchVelocity = 0.f;

	const FVector* Rest = HandRestLocations.Find(HandLId);
	const FVector LocalFist = (Rest ? *Rest : FVector::ZeroVector) + FVector(Tuning.PunchReach, 0.f, 0.f) + PunchSideOffset;
	const FVector Fist = GetActorTransform().TransformPosition(LocalFist);

	// 주먹 위치 반경 안의 가장 가까운 적
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Fist, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Tuning.PunchRadius),
		FCollisionQueryParams(SCENE_QUERY_STAT(CRPunch), false, this));
	ACRCharacter* Best = nullptr;
	float BestDistance = UE_BIG_NUMBER;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACRCharacter* Candidate = Cast<ACRCharacter>(Overlap.GetActor());
		const float Distance = Candidate ? FVector::DistSquared(Candidate->GetActorLocation(), Fist) : UE_BIG_NUMBER;
		if (Candidate && Candidate != this && !Candidate->IsDead() && Distance < BestDistance)
		{
			Best = Candidate;
			BestDistance = Distance;
		}
	}
	if (Best)
	{
		FHitResult Hit(Best, Best->GetCapsuleComponent(), Fist, -GetActorForwardVector());
		Hit.bBlockingHit = true;
		CRCombat::ApplyWeaponHit(this, Best, ECRWeaponType::Punch, Hit);
		return;
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ACRExplosiveBarrel* Barrel = Cast<ACRExplosiveBarrel>(Overlap.GetActor()))
		{
			Barrel->ApplyHit(2, this);
			return;
		}
	}

	// 적이 없으면 손 → 주먹 방향으로 블럭 검사 (블럭 HP 2)
	const FVector HandStart = GetActorTransform().TransformPosition(Rest ? *Rest : FVector::ZeroVector);
	FHitResult BlockHit;
	FCollisionObjectQueryParams Statics;
	Statics.AddObjectTypesToQuery(ECC_WorldStatic);
	if (GetWorld()->SweepSingleByObjectType(BlockHit, HandStart, Fist + GetActorForwardVector() * Tuning.PunchRadius, FQuat::Identity, Statics,
		FCollisionShape::MakeSphere(10.f), FCollisionQueryParams(SCENE_QUERY_STAT(CRPunchBlock), false, this)))
	{
		if (ACRBlockArena* Arena = ACRBlockArena::FromComponent(BlockHit.GetComponent()))
		{
			Arena->DamageAt(BlockHit.ImpactPoint, BlockHit.ImpactNormal, 2);
			if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
			{
				const UCRTuningSettings& Feel = UCRTuningSubsystem::GetSettings(this);
				if (Feel.bHitStar)
				{
					Fx->SpawnHitStar(BlockHit.ImpactPoint, 45.f, FLinearColor(1.f, 0.9f, 0.4f));
				}
			}
		}
	}
}

void ACRCharacter::UpdatePunch(float DeltaSeconds)
{
	PunchClock += DeltaSeconds;
	if (PunchExtend != 0.f || PunchVelocity != 0.f)
	{
		// 스프링으로 되돌아온다 (돌아올 때도 빠르면 스미어)
		const float Accel = -PunchStiffness * PunchExtend - PunchDamping * PunchVelocity;
		PunchVelocity += Accel * DeltaSeconds;
		PunchExtend += PunchVelocity * DeltaSeconds;
		if (FMath::Abs(PunchExtend) < 0.002f && FMath::Abs(PunchVelocity) < 0.02f)
		{
			PunchExtend = 0.f;
			PunchVelocity = 0.f;
		}
	}

	// 달리기: 가장자리 집중선
	if (bSprinting && IsPlayerControlled() && GetVelocity().Size2D() > SprintFocusMinSpeed)
	{
		if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(this))
		{
			Screen->SetSustainedFocus(UCRTuningSubsystem::GetSettings(this).SprintFocus);
		}
	}
}

void ACRCharacter::PlayHitFlash(float Duration, float Intensity)
{
	FlashDuration = FMath::Max(Duration, 0.001f);
	FlashTimeLeft = FlashDuration;
	FlashIntensity = Intensity;
	SetPartsFlash(Intensity);
}

void ACRCharacter::ApplyKnockback(const FVector& Velocity)
{
	if (bPendingDeathFx)
	{
		StartDeathFx(Velocity, true); // 죽는 타격: 더 세게 밀리고 투구/무기가 벗겨진다
		return;
	}
	LaunchCharacter(Velocity, false, true);
}

bool ACRCharacter::GetWeaponAimPoint(FVector& OutPoint) const
{
	if (IsPlayerControlled() && ViewComponent)
	{
		OutPoint = ViewComponent->GetAimPoint();
		return true;
	}
	if (const ACRCharacter* Enemy = TargetEnemy.Get())
	{
		OutPoint = Enemy->GetActorLocation() + FVector(0.f, 0.f, 20.f);
		return true;
	}
	return false;
}

void ACRCharacter::SetPartsFlash(float Value)
{
	if (!bUseCustomPrimitiveData)
	{
		return;
	}
	for (UStaticMeshComponent* Part : PartComponents)
	{
		if (Part)
		{
			Part->SetCustomPrimitiveDataFloat(CRMaterialData::FlashIndex, Value);
		}
	}
}

void ACRCharacter::UpdateFlash(float DeltaSeconds)
{
	if (FlashTimeLeft <= 0.f)
	{
		return;
	}
	// 히트스톱 중(시간 배율 낮음)에는 번쩍임도 유지된다
	FlashTimeLeft -= DeltaSeconds;
	const float Alpha = FMath::Clamp(FlashTimeLeft / FlashDuration, 0.f, 1.f);
	SetPartsFlash(FlashIntensity * Alpha * Alpha);
}

void ACRCharacter::UpdateFall(float DeltaSeconds)
{
	const float Target = bDead ? 1.f : 0.f;
	if (FallAlpha == Target)
	{
		return;
	}
	FallAlpha = bDead
		? FMath::Min(1.f, FallAlpha + DeltaSeconds * FallSpeed)
		: FMath::Max(0.f, FallAlpha - DeltaSeconds * ReviveSpeed);

	// 바닥(캡슐 아래끝)을 축으로 기울인다
	const float Eased = bDead ? 1.f - FMath::Square(1.f - FallAlpha) : FallAlpha * FallAlpha;
	const FQuat Tilt(FallAxis, FMath::DegreesToRadians(FallAngle * Eased));
	const FVector Pivot(0.f, 0.f, -CapsuleHalfHeight);
	VisualRoot->SetRelativeLocationAndRotation(Pivot + Tilt.RotateVector(-Pivot), Tilt);
}

void ACRCharacter::UpdateHands(float DeltaSeconds)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	HandBobTime += DeltaSeconds * Tuning.HandBobSpeed * UE_TWO_PI;

	FVector WeaponHandLocation = FVector::ZeroVector;
	FRotator WeaponHandRotation = FRotator::ZeroRotator;
	const bool bWeaponPose = WeaponComponent && WeaponComponent->GetHandPose(WeaponHandLocation, WeaponHandRotation);

	for (const TPair<FName, FVector>& Rest : HandRestLocations)
	{
		UStaticMeshComponent* Hand = FindPart(Rest.Key);
		if (!Hand)
		{
			continue;
		}

		// 양손이 반 박자 어긋나게 둥둥 뜬다
		const float Phase = Rest.Key == HandLId ? 0.f : UE_PI;
		const FVector Bob(0.f, 0.f, FMath::Sin(HandBobTime + Phase) * Tuning.HandBobAmplitude);

		if (Rest.Key == HandRId && bWeaponPose)
		{
			Hand->SetRelativeLocationAndRotation(WeaponHandLocation + Bob * 0.3f, WeaponHandRotation);
		}
		else
		{
			FVector Location = Rest.Value + Bob;
			if (Rest.Key == HandLId && PunchExtend != 0.f)
			{
				const UCRTuningSettings& PunchTuning = UCRTuningSubsystem::GetSettings(this);
				Location += (FVector(PunchTuning.PunchReach, 0.f, 0.f) + PunchSideOffset) * PunchExtend;
			}
			Hand->SetRelativeLocationAndRotation(Location, FRotator::ZeroRotator);
		}
	}
}

void ACRCharacter::UpdateFacing(float DeltaSeconds)
{
	if (!IsPlayerControlled() || bDead)
	{
		return;
	}
	float DesiredYaw;
	if (!ViewComponent->GetDesiredFacingYaw(DesiredYaw))
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const FRotator Current = GetActorRotation();
	const FRotator Target(0.f, DesiredYaw, 0.f);
	SetActorRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, Tuning.TurnSpeed));
}
