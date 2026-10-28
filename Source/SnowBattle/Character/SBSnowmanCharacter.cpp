#include "Character/SBSnowmanCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/SBViewComponent.h"
#include "Character/SBSnowmanAppearance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SBGameSettings.h"
#include "Core/SBTargetSpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/SBAttributeSet.h"
#include "GAS/SBGameplayTags.h"
#include "GAS/SBWeaponAbilities.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Weapon/SBWeaponComponent.h"

namespace
{
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
}

ASBSnowmanCharacter::ASBSnowmanCharacter()
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

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	ViewComponent = CreateDefaultSubobject<USBViewComponent>(TEXT("View"));
	WeaponComponent = CreateDefaultSubobject<USBWeaponComponent>(TEXT("Weapon"));

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<USBAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* ASBSnowmanCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ASBSnowmanCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildParts();
}

void ASBSnowmanCharacter::BeginPlay()
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
	if (USBTuningSubsystem* Tuning = USBTuningSubsystem::Get(this))
	{
		TuningChangedHandle = Tuning->OnChanged.AddUObject(this, &ASBSnowmanCharacter::ApplyTuning);
	}

	if (IsPlayerControlled())
	{
		WeaponComponent->Equip(ESBWeaponType::Sword);
	}
}

void ASBSnowmanCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasActorBegunPlay() && IsPlayerControlled() && WeaponComponent->GetEquipped() == ESBWeaponType::None)
	{
		WeaponComponent->Equip(ESBWeaponType::Sword);
	}
}

void ASBSnowmanCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USBTuningSubsystem* Tuning = USBTuningSubsystem::Get(this))
	{
		Tuning->OnChanged.Remove(TuningChangedHandle);
	}
	GetWorldTimerManager().ClearTimer(ReviveTimer);
	Super::EndPlay(EndPlayReason);
}

void ASBSnowmanCharacter::InitAbilitySystem()
{
	AbilitySystem->InitAbilityActorInfo(this, this);

	if (!bAbilitiesGranted && HasAuthority())
	{
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(USBGA_SwordSwing::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(USBGA_LaserFire::StaticClass()));
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(USBGA_ProjectileFire::StaticClass()));
		bAbilitiesGranted = true;
	}

	RestoreFullHealth();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(USBAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ASBSnowmanCharacter::HandleHealthChanged);
}

float ASBSnowmanCharacter::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ASBSnowmanCharacter::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ASBSnowmanCharacter::GetTunedMaxHealth() const
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	return bMonster ? Tuning.MonsterMaxHealth : Tuning.MaxHealth;
}

void ASBSnowmanCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0.f && !bDead)
	{
		Die();
	}
}

void ASBSnowmanCharacter::Die()
{
	bDead = true;
	AbilitySystem->AddLooseGameplayTag(SBTags::State_Dead);
	WeaponComponent->SetTriggerHeld(false);
	TargetEnemy.Reset();

	if (Team != ESBTeam::None)
	{
		if (USBTargetSpawnerSubsystem* Spawner = USBTargetSpawnerSubsystem::Get(this))
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

	const float Delay = USBTuningSubsystem::GetSettings(this).RespawnDelay;
	GetWorldTimerManager().SetTimer(ReviveTimer, this, &ASBSnowmanCharacter::Revive, FMath::Max(Delay, 0.01f), false);
}

void ASBSnowmanCharacter::Revive()
{
	if (bMonster)
	{
		// 몬스터는 쓰러진 자리에서 일어나지 않고 하늘에서 새로 떨어진다
		Respawn();
		ResetPose();
	}
	RestoreFullHealth();
	AbilitySystem->RemoveLooseGameplayTag(SBTags::State_Dead);
	bDead = false;
}

void ASBSnowmanCharacter::RestoreFullHealth()
{
	const float MaxHealth = GetTunedMaxHealth();
	AbilitySystem->SetNumericAttributeBase(USBAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystem->SetNumericAttributeBase(USBAttributeSet::GetHealthAttribute(), MaxHealth);
}

void ASBSnowmanCharacter::ResetPose()
{
	FallAlpha = 0.f;
	VisualRoot->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
}

void ASBSnowmanCharacter::Respawn()
{
	FVector Location = HomeLocation;
	if (bMonster)
	{
		if (const USBTargetSpawnerSubsystem* Spawner = USBTargetSpawnerSubsystem::Get(this))
		{
			Spawner->FindRespawnLocation(Team, SkyDropHeight, Location);
		}
	}
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}

void ASBSnowmanCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	// 엔진 기본 동작(액터 삭제) 대신 다시 등장
	Respawn();
}

void ASBSnowmanCharacter::Tick(float DeltaSeconds)
{
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

	UpdateChase(DeltaSeconds);
	UpdateHands(DeltaSeconds);
	UpdateFacing(DeltaSeconds);
	UpdateFlash(DeltaSeconds);
	UpdateFall(DeltaSeconds);
}

void ASBSnowmanCharacter::UpdateChase(float DeltaSeconds)
{
	if (!bMonster || bDead)
	{
		return;
	}
	if (Team != ESBTeam::None)
	{
		UpdateBattle(DeltaSeconds);
		return;
	}
	const float Speed = USBTuningSubsystem::GetSettings(this).MonsterMoveSpeed;
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

void ASBSnowmanCharacter::UpdateBattle(float DeltaSeconds)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

	// 가장 가까운 적은 가끔만 다시 찾는다 (다 같이 같은 프레임에 찾지 않도록 간격을 흩뜨림)
	RetargetTimeLeft -= DeltaSeconds;
	ASBSnowmanCharacter* Enemy = TargetEnemy.Get();
	if (RetargetTimeLeft <= 0.f || !Enemy || Enemy->IsDead() || Enemy->IsInPool())
	{
		const USBTargetSpawnerSubsystem* Spawner = USBTargetSpawnerSubsystem::Get(this);
		Enemy = Spawner ? Spawner->FindNearestEnemy(this) : nullptr;
		TargetEnemy = Enemy;
		RetargetTimeLeft = FMath::FRandRange(0.2f, 0.4f);
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

	// 사거리 안이면 휘두르고(간격은 무기 쪽에서), 너무 붙지 않게 조금 앞에서 멈춘다
	WeaponComponent->SetTriggerHeld(Distance <= Tuning.BattleAttackRange);
	if (Distance > Tuning.BattleAttackRange * 0.8f)
	{
		AddMovementInput(Direction, FMath::Clamp(Tuning.BattleMoveSpeed / GetCharacterMovement()->MaxWalkSpeed, 0.f, 1.f));
	}
}

void ASBSnowmanCharacter::SetupAsMonster(const FLinearColor& InAccentColor, ESBTeam InTeam)
{
	bMonster = true;
	Team = InTeam;
	TargetEnemy.Reset();
	RetargetTimeLeft = 0.f;
	SetAccentColor(InAccentColor);

	// 몬스터에겐 카메라가 필요 없다 (수백 마리일 때 틱 비용 절약). 무기는 팀 전투일 때만.
	SpringArm->SetComponentTickEnabled(false);
	ViewComponent->SetComponentTickEnabled(false);
	const bool bArmed = Team != ESBTeam::None;
	WeaponComponent->SetTriggerHeld(false);
	WeaponComponent->Equip(bArmed ? ESBWeaponType::Sword : ESBWeaponType::None);
	WeaponComponent->SetComponentTickEnabled(bArmed);
	ApplyTuning();
	RestoreFullHealth();
}

void ASBSnowmanCharacter::SetSpectating(bool bSpectating)
{
	SetActorHiddenInGame(bSpectating);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bSpectating ? ECR_Ignore : ECR_Block);
	WeaponComponent->SetTriggerHeld(false);
}

void ASBSnowmanCharacter::SetAccentColor(const FLinearColor& InAccentColor)
{
	AccentColor = InAccentColor;
	for (UStaticMeshComponent* Part : PartComponents)
	{
		if (Part && Part->ComponentTags.Contains(AccentTag))
		{
			if (bUseCustomPrimitiveData)
			{
				Part->SetCustomPrimitiveDataVector4(SBMaterialData::ColorIndex, FVector4(AccentColor.R, AccentColor.G, AccentColor.B, 1.f));
			}
			else if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
			{
				Material->SetVectorParameterValue(ColorParam, AccentColor);
			}
		}
	}
}

void ASBSnowmanCharacter::OnPoolAcquired()
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
		AbilitySystem->RemoveLooseGameplayTag(SBTags::State_Dead, AbilitySystem->GetTagCount(SBTags::State_Dead));
	}
	bDead = false;
	ResetPose();
	FlashTimeLeft = 0.f;
	SetPartsFlash(0.f);
}

void ASBSnowmanCharacter::OnPoolReleased()
{
	bInPool = true;
	Team = ESBTeam::None;
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

void ASBSnowmanCharacter::MoveInput(const FVector2D& Axis)
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

void ASBSnowmanCharacter::LookInput(const FVector2D& Axis)
{
	if (ViewComponent->GetViewMode() != ESBViewMode::Shoulder)
	{
		return;
	}
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	const float Scale = LookScale * Tuning.MouseSensitivity;
	AddControllerYawInput(Axis.X * Scale);
	AddControllerPitchInput(Axis.Y * Scale * (Tuning.bInvertMouseY ? -1.f : 1.f));
}

void ASBSnowmanCharacter::ToggleViewMode()
{
	ViewComponent->ToggleViewMode();
}

UStaticMeshComponent* ASBSnowmanCharacter::FindPart(FName Id) const
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

void ASBSnowmanCharacter::BuildParts()
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
	HandRestLocations.Reset();

	const TArray<FSBSnowmanPartSpec>& Specs = Appearance ? Appearance->Parts : USBSnowmanAppearance::MakeDefaultParts();

	// 우선순위: 외형 에셋 머티리얼 > 프로젝트 설정 캐릭터 머티리얼(CPD) > 엔진 기본(MID)
	UMaterialInterface* BaseMaterial = Appearance && !Appearance->Material.IsNull() ? Appearance->Material.LoadSynchronous() : nullptr;
	if (!BaseMaterial)
	{
		BaseMaterial = USBGameSettings::LoadCharacterMaterial();
	}
	bUseCustomPrimitiveData = BaseMaterial != nullptr;
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
	}

	for (const FSBSnowmanPartSpec& Spec : Specs)
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
		Part->RegisterComponent();

		const FLinearColor Color = Spec.bUseAccentColor ? AccentColor : Spec.Color;
		if (bUseCustomPrimitiveData)
		{
			Part->SetMaterial(0, BaseMaterial);
			Part->SetCustomPrimitiveDataVector4(SBMaterialData::ColorIndex, FVector4(Color.R, Color.G, Color.B, 1.f));
			Part->SetCustomPrimitiveDataFloat(SBMaterialData::FlashIndex, 0.f);
		}
		else if (BaseMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Part);
			Material->SetVectorParameterValue(ColorParam, Color);
			Part->SetMaterial(0, Material);
		}

		if (Spec.Id == HandLId || Spec.Id == HandRId)
		{
			HandRestLocations.Add(Spec.Id, Spec.Transform.GetLocation());
		}
		PartComponents.Add(Part);
	}
}

void ASBSnowmanCharacter::ApplyTuning()
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	// 몬스터는 이동 입력 크기로 속도를 줄이므로 최대 속도는 둘 중 큰 값
	GetCharacterMovement()->MaxWalkSpeed = bMonster
		? FMath::Max3(Tuning.MoveSpeed, Tuning.MonsterMoveSpeed, Tuning.BattleMoveSpeed)
		: Tuning.MoveSpeed;
	GetCharacterMovement()->MaxAcceleration = Tuning.MaxAcceleration;

	// 최대 체력이 바뀌면 비율 유지
	if (AbilitySystem && AbilitySystem->AbilityActorInfo.IsValid() && !bDead)
	{
		const float OldMax = GetMaxHealth();
		const float NewMax = GetTunedMaxHealth();
		if (!FMath::IsNearlyEqual(OldMax, NewMax) && OldMax > 0.f)
		{
			const float Ratio = GetHealth() / OldMax;
			AbilitySystem->SetNumericAttributeBase(USBAttributeSet::GetMaxHealthAttribute(), NewMax);
			AbilitySystem->SetNumericAttributeBase(USBAttributeSet::GetHealthAttribute(), FMath::Max(1.f, NewMax * Ratio));
		}
	}
}

void ASBSnowmanCharacter::PlayHitFlash(float Duration, float Intensity)
{
	FlashDuration = FMath::Max(Duration, 0.001f);
	FlashTimeLeft = FlashDuration;
	FlashIntensity = Intensity;
	SetPartsFlash(Intensity);
}

void ASBSnowmanCharacter::ApplyKnockback(const FVector& Velocity)
{
	LaunchCharacter(Velocity, false, true);
}

void ASBSnowmanCharacter::SetPartsFlash(float Value)
{
	if (!bUseCustomPrimitiveData)
	{
		return;
	}
	for (UStaticMeshComponent* Part : PartComponents)
	{
		if (Part)
		{
			Part->SetCustomPrimitiveDataFloat(SBMaterialData::FlashIndex, Value);
		}
	}
}

void ASBSnowmanCharacter::UpdateFlash(float DeltaSeconds)
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

void ASBSnowmanCharacter::UpdateFall(float DeltaSeconds)
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

void ASBSnowmanCharacter::UpdateHands(float DeltaSeconds)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
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
			Hand->SetRelativeLocationAndRotation(Rest.Value + Bob, FRotator::ZeroRotator);
		}
	}
}

void ASBSnowmanCharacter::UpdateFacing(float DeltaSeconds)
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
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	const FRotator Current = GetActorRotation();
	const FRotator Target(0.f, DesiredYaw, 0.f);
	SetActorRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, Tuning.TurnSpeed));
}
