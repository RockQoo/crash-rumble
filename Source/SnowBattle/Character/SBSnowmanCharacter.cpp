#include "Character/SBSnowmanCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/SBViewComponent.h"
#include "Character/SBSnowmanAppearance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* DefaultMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName ColorParam(TEXT("Color"));
	const FName HandLId(TEXT("HandL"));
	const FName HandRId(TEXT("HandR"));

	// 마우스 픽셀 이동량 → 각도
	constexpr float LookScale = 0.15f;
}

ASBSnowmanCharacter::ASBSnowmanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(50.f, 100.f);

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

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	ViewComponent = CreateDefaultSubobject<USBViewComponent>(TEXT("View"));
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

	ViewComponent->Setup(SpringArm, Camera);
	ApplyTuning();
	if (USBTuningSubsystem* Tuning = USBTuningSubsystem::Get(this))
	{
		TuningChangedHandle = Tuning->OnChanged.AddUObject(this, &ASBSnowmanCharacter::ApplyTuning);
	}
}

void ASBSnowmanCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USBTuningSubsystem* Tuning = USBTuningSubsystem::Get(this))
	{
		Tuning->OnChanged.Remove(TuningChangedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ASBSnowmanCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHands(DeltaSeconds);
	UpdateFacing(DeltaSeconds);
}

void ASBSnowmanCharacter::MoveInput(const FVector2D& Axis)
{
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
	UMaterialInterface* BaseMaterial = Appearance && !Appearance->Material.IsNull()
		? Appearance->Material.LoadSynchronous()
		: LoadObject<UMaterialInterface>(nullptr, DefaultMaterialPath);

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
		Part->SetupAttachment(RootComponent);
		Part->SetRelativeTransform(Spec.Transform);
		Part->RegisterComponent();

		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Part);
			Material->SetVectorParameterValue(ColorParam, Spec.bUseAccentColor ? AccentColor : Spec.Color);
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
	GetCharacterMovement()->MaxWalkSpeed = Tuning.MoveSpeed;
	GetCharacterMovement()->MaxAcceleration = Tuning.MaxAcceleration;
}

void ASBSnowmanCharacter::UpdateHands(float DeltaSeconds)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	HandBobTime += DeltaSeconds * Tuning.HandBobSpeed * UE_TWO_PI;

	for (const TPair<FName, FVector>& Rest : HandRestLocations)
	{
		// 양손이 반 박자 어긋나게 둥둥 뜬다
		const float Phase = Rest.Key == HandLId ? 0.f : UE_PI;
		const FVector Offset(0.f, 0.f, FMath::Sin(HandBobTime + Phase) * Tuning.HandBobAmplitude);
		if (UStaticMeshComponent* Hand = FindPart(Rest.Key))
		{
			Hand->SetRelativeLocation(Rest.Value + Offset);
		}
	}
}

void ASBSnowmanCharacter::UpdateFacing(float DeltaSeconds)
{
	if (!IsPlayerControlled())
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
