#include "Core/CRTitlePlayerController.h"

#include "Camera/CameraActor.h"
#include "Character/CRCharacter.h"
#include "Core/CRModes.h"
#include "Core/SCRTitleScreen.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	constexpr int32 TitleZOrder = 100;
	constexpr float OrbitRadius = 520.f;
	constexpr float OrbitHeight = 170.f;
	constexpr float OrbitDegreesPerSecond = 7.f;
	// 메뉴가 왼쪽을 가리므로 캐릭터가 화면 오른쪽에 오도록 시선을 왼쪽으로 민다
	constexpr float LookSideOffset = 190.f;
}

ACRTitlePlayerController::ACRTitlePlayerController()
{
	bAutoManageActiveCameraTarget = false;
}

void ACRTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// 배경 중심: 맵에 놓인 캐릭터 → 플레이어 시작점 → 원점
	if (TActorIterator<ACRCharacter> It(GetWorld()); It)
	{
		OrbitCenter = It->GetActorLocation();
	}
	else if (TActorIterator<APlayerStart> Start(GetWorld()); Start)
	{
		OrbitCenter = Start->GetActorLocation();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OrbitCamera = GetWorld()->SpawnActor<ACameraActor>(Params);
	if (OrbitCamera)
	{
		PlayerTick(0.f);
		SetViewTarget(OrbitCamera);
	}

	if (GEngine && GEngine->GameViewport)
	{
		Screen = SNew(SCRTitleScreen)
			.OnModeSelected(FCROnModeSelected::CreateUObject(this, &ACRTitlePlayerController::HandleModeSelected))
			.OnQuit(FSimpleDelegate::CreateUObject(this, &ACRTitlePlayerController::HandleQuit));
		GEngine->GameViewport->AddViewportWidgetContent(Screen.ToSharedRef(), TitleZOrder);

		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Screen);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
}

void ACRTitlePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Screen.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
	Super::EndPlay(EndPlayReason);
}

void ACRTitlePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!OrbitCamera)
	{
		return;
	}
	OrbitYaw = FMath::Fmod(OrbitYaw + OrbitDegreesPerSecond * DeltaTime, 360.f);
	const FVector Offset = FRotator(0.f, OrbitYaw, 0.f).Vector() * OrbitRadius + FVector(0.f, 0.f, OrbitHeight);
	const FVector CameraLocation = OrbitCenter + Offset;
	const FVector Forward = (OrbitCenter - CameraLocation).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector LookAt = OrbitCenter + FVector(0.f, 0.f, 40.f) - Right * LookSideOffset;
	OrbitCamera->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
}

void ACRTitlePlayerController::HandleModeSelected(ECRMode Mode)
{
	CRModes::Open(this, Mode);
}

void ACRTitlePlayerController::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
