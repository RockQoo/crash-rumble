#include "Camera/CRFreeCameraPawn.h"

#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "GameFramework/FloatingPawnMovement.h"

ACRFreeCameraPawn::ACRFreeCameraPawn()
{
	// 옛 입력 바인딩(DefaultPawn_MoveForward 등) 대신 컨트롤러의 Enhanced Input을 쓴다
	bAddDefaultMovementBindings = false;
	PrimaryActorTick.bCanEverTick = true;
}

void ACRFreeCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (UFloatingPawnMovement* Movement = Cast<UFloatingPawnMovement>(GetMovementComponent()))
	{
		const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
		Movement->MaxSpeed = Tuning.FreeCameraSpeed * (bFast ? Tuning.FreeCameraFastMultiplier : 1.f);
		Movement->Acceleration = Movement->MaxSpeed * 8.f;
		Movement->Deceleration = Movement->MaxSpeed * 8.f;
	}
}
