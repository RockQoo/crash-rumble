#include "Camera/SBViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Core/SBTuningSettings.h"
#include "Core/SBTuningSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/SBCombat.h"
#include "Misc/App.h"

namespace
{
	constexpr float AimTraceDistance = 20000.f;
	constexpr float MinFacingDistance = 30.f;
}

USBViewComponent::USBViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USBViewComponent::Setup(USpringArmComponent* InSpringArm, UCameraComponent* InCamera)
{
	SpringArm = InSpringArm;
	Camera = InCamera;

	// 회전은 이 컴포넌트가 직접 계산한다
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->bDoCollisionTest = true;
	SpringArm->bEnableCameraLag = false;
	SpringArm->AddTickPrerequisiteComponent(this);
	Camera->bUsePawnControlRotation = false;
}

void USBViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (SpringArm && Camera)
	{
		UpdateCamera(DeltaTime);
		// 히트스톱으로 오너 시간이 멈춰도 흔들림/반동은 실시간으로 진행
		UpdateCameraOffset(FApp::GetDeltaTime());
		UpdateAim();
	}
}

void USBViewComponent::AddShake(float Scale)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);
	ShakeAmplitude = FMath::Max(ShakeAmplitude * (ShakeTimeLeft > 0.f ? 1.f : 0.f), Tuning.CameraShakeIntensity * Scale);
	ShakeTimeLeft = Tuning.CameraShakeDuration;
}

void USBViewComponent::AddRecoil(float PitchDegrees)
{
	RecoilPitchOffset = FMath::Min(RecoilPitchOffset + PitchDegrees, 10.f);
}

void USBViewComponent::UpdateCameraOffset(float RealDeltaTime)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

	FRotator Offset(RecoilPitchOffset, 0.f, 0.f);
	RecoilPitchOffset = FMath::FInterpTo(RecoilPitchOffset, 0.f, RealDeltaTime, Tuning.CameraRecoilRecover);

	if (ShakeTimeLeft > 0.f)
	{
		ShakeClock += RealDeltaTime * Tuning.CameraShakeFrequency;
		const float Fade = FMath::Square(ShakeTimeLeft / FMath::Max(Tuning.CameraShakeDuration, 0.001f));
		const float Amp = ShakeAmplitude * Fade;
		Offset.Pitch += FMath::PerlinNoise1D(ShakeClock) * Amp;
		Offset.Yaw += FMath::PerlinNoise1D(ShakeClock + 37.f) * Amp;
		Offset.Roll += FMath::PerlinNoise1D(ShakeClock + 71.f) * Amp * 0.5f;
		ShakeTimeLeft -= RealDeltaTime;
		if (ShakeTimeLeft <= 0.f)
		{
			ShakeAmplitude = 0.f;
		}
	}

	Camera->SetRelativeRotation(Offset);
}

void USBViewComponent::ToggleViewMode()
{
	if (ViewMode == ESBViewMode::Shoulder)
	{
		ViewMode = ESBViewMode::TopDown;
		TopDownYaw = GetControlRotation().Yaw;
	}
	else
	{
		ViewMode = ESBViewMode::Shoulder;
		// 숄더뷰로 돌아올 때 탑다운에서 보던 방향을 이어간다
		if (const APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(FRotator(-10.f, Pawn->GetActorRotation().Yaw, 0.f));
			}
		}
	}
	OnViewModeChanged.Broadcast(ViewMode);
}

float USBViewComponent::GetMovementYaw() const
{
	return SpringArm ? SpringArm->GetComponentRotation().Yaw : 0.f;
}

bool USBViewComponent::GetDesiredFacingYaw(float& OutYaw) const
{
	if (ViewMode == ESBViewMode::Shoulder)
	{
		OutYaw = GetControlRotation().Yaw;
		return true;
	}

	FVector ToAim = AimPoint - GetOwner()->GetActorLocation();
	ToAim.Z = 0.f;
	if (ToAim.SizeSquared() < FMath::Square(MinFacingDistance))
	{
		return false;
	}
	OutYaw = ToAim.Rotation().Yaw;
	return true;
}

void USBViewComponent::UpdateCamera(float DeltaTime)
{
	const USBTuningSettings& Tuning = USBTuningSubsystem::GetSettings(this);

	const float Target = ViewMode == ESBViewMode::TopDown ? 1.f : 0.f;
	BlendAlpha = FMath::FInterpTo(BlendAlpha, Target, DeltaTime, Tuning.ViewBlendSpeed);
	if (FMath::IsNearlyEqual(BlendAlpha, Target, 0.001f))
	{
		BlendAlpha = Target;
	}

	const FQuat ShoulderRotation = GetControlRotation().Quaternion();
	const FQuat TopDownRotation = FRotator(-Tuning.TopDownPitch, TopDownYaw, 0.f).Quaternion();
	SpringArm->SetWorldRotation(FQuat::Slerp(ShoulderRotation, TopDownRotation, BlendAlpha));

	SpringArm->TargetArmLength = FMath::Lerp(Tuning.ShoulderArmLength, Tuning.TopDownArmLength, BlendAlpha);
	const FVector ShoulderOffset(0.f, Tuning.ShoulderOffsetY, Tuning.ShoulderOffsetZ);
	SpringArm->SocketOffset = FMath::Lerp(ShoulderOffset, FVector::ZeroVector, BlendAlpha);
	Camera->SetFieldOfView(FMath::Lerp(Tuning.ShoulderFOV, Tuning.TopDownFOV, BlendAlpha));
}

void USBViewComponent::UpdateAim()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		AimPoint = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 1000.f;
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBAim), false, GetOwner());
	const FCollisionObjectQueryParams ObjectParams = SBCombat::GetWeaponQueryParams();
	FHitResult Hit;

	if (ViewMode == ESBViewMode::Shoulder)
	{
		// 반동/흔들림 오프셋을 뺀 기준 방향으로 조준
		const FVector Start = SpringArm->GetSocketTransform(USpringArmComponent::SocketName).GetLocation();
		const FVector End = Start + SpringArm->GetComponentRotation().Vector() * AimTraceDistance;
		AimPoint = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, Params) ? FVector(Hit.ImpactPoint) : End;
		return;
	}

	FVector Origin, Direction;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return;
	}
	const FVector End = Origin + Direction * AimTraceDistance;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Origin, End, ObjectParams, Params))
	{
		AimPoint = Hit.ImpactPoint;
	}
	else
	{
		// 허공이면 캐릭터 높이의 수평면과 교차
		const FPlane Ground(GetOwner()->GetActorLocation(), FVector::UpVector);
		AimPoint = FMath::RayPlaneIntersection(Origin, Direction, Ground);
	}
}

FRotator USBViewComponent::GetControlRotation() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : GetOwner()->GetActorRotation();
}
