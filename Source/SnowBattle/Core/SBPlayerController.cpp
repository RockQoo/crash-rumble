#include "Core/SBPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Camera/SBViewComponent.h"
#include "Character/SBSnowmanCharacter.h"
#include "Core/SBTuningSubsystem.h"
#include "Core/SSBTuningPanel.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Styling/CoreStyle.h"
#include "Weapon/SBWeaponComponent.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr int32 OptionsZOrder = 100;
	constexpr int32 HelpZOrder = 10;
}

void ASBPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -70.f;
		PlayerCameraManager->ViewPitchMax = 60.f;
	}

	if (GEngine && GEngine->GameViewport)
	{
		HelpOverlay = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(16.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("WASD 이동 · 마우스 시점 · 좌클릭 공격(꾹 누르면 연사) · 1 칼 / 2 레이저 / 3 투사체 · V 시점 전환 · Tab 튜닝 옵션")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
				.ShadowOffset(FVector2D(1.f, 1.f))
			];
		GEngine->GameViewport->AddViewportWidgetContent(HelpOverlay.ToSharedRef(), HelpZOrder);
	}

	RefreshInputMode();
}

void ASBPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (OptionsPanel.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(OptionsPanel.ToSharedRef());
		}
		if (HelpOverlay.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(HelpOverlay.ToSharedRef());
		}
	}
	OptionsPanel.Reset();
	HelpOverlay.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASBPlayerController::CreateInputAssets()
{
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type, bool bWhenPaused = false)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		Action->bTriggerWhenPaused = bWhenPaused;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	ToggleViewAction = MakeAction(TEXT("IA_ToggleView"), EInputActionValueType::Boolean);
	OptionsAction = MakeAction(TEXT("IA_Options"), EInputActionValueType::Boolean, true);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	SwordAction = MakeAction(TEXT("IA_Weapon1"), EInputActionValueType::Boolean);
	LaserAction = MakeAction(TEXT("IA_Weapon2"), EInputActionValueType::Boolean);
	ProjectileAction = MakeAction(TEXT("IA_Weapon3"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));

	// WASD → (X = 오른쪽, Y = 앞)
	auto MapMoveKey = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	MapMoveKey(EKeys::W, true, false);
	MapMoveKey(EKeys::S, true, true);
	MapMoveKey(EKeys::A, false, true);
	MapMoveKey(EKeys::D, false, false);

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(ToggleViewAction, EKeys::V);
	MappingContext->MapKey(OptionsAction, EKeys::Tab);
	MappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(SwordAction, EKeys::One);
	MappingContext->MapKey(LaserAction, EKeys::Two);
	MappingContext->MapKey(ProjectileAction, EKeys::Three);
}

void ASBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateInputAssets();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASBPlayerController::OnMove);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASBPlayerController::OnLook);
		Input->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ASBPlayerController::OnToggleView);
		Input->BindAction(OptionsAction, ETriggerEvent::Started, this, &ASBPlayerController::OnToggleOptions);
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &ASBPlayerController::OnFirePressed);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ASBPlayerController::OnFireReleased);
		Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &ASBPlayerController::OnFireReleased);
		Input->BindAction(SwordAction, ETriggerEvent::Started, this, &ASBPlayerController::OnSelectSword);
		Input->BindAction(LaserAction, ETriggerEvent::Started, this, &ASBPlayerController::OnSelectLaser);
		Input->BindAction(ProjectileAction, ETriggerEvent::Started, this, &ASBPlayerController::OnSelectProjectile);
	}
}

void ASBPlayerController::SetTriggerHeld(bool bHeld)
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->GetWeaponComponent()->SetTriggerHeld(bHeld);
	}
}

void ASBPlayerController::OnFirePressed()
{
	SetTriggerHeld(true);
}

void ASBPlayerController::OnFireReleased()
{
	SetTriggerHeld(false);
}

void ASBPlayerController::OnSelectSword()
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ESBWeaponType::Sword);
	}
}

void ASBPlayerController::OnSelectLaser()
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ESBWeaponType::Laser);
	}
}

void ASBPlayerController::OnSelectProjectile()
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ESBWeaponType::Projectile);
	}
}

void ASBPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ASBSnowmanCharacter* Snowman = Cast<ASBSnowmanCharacter>(InPawn))
	{
		Snowman->GetViewComponent()->OnViewModeChanged.AddUObject(this, &ASBPlayerController::HandleViewModeChanged);
	}
	RefreshInputMode();
}

void ASBPlayerController::OnMove(const FInputActionValue& Value)
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->MoveInput(Value.Get<FVector2D>());
	}
}

void ASBPlayerController::OnLook(const FInputActionValue& Value)
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->LookInput(Value.Get<FVector2D>());
	}
}

void ASBPlayerController::OnToggleView()
{
	if (ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>())
	{
		Snowman->ToggleViewMode();
	}
}

void ASBPlayerController::OnToggleOptions()
{
	ToggleOptions();
}

void ASBPlayerController::ToggleOptions()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	if (OptionsPanel.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OptionsPanel.ToSharedRef());
		OptionsPanel.Reset();
		if (USBTuningSubsystem* Tuning = USBTuningSubsystem::Get(this))
		{
			Tuning->Save();
		}
		SetPause(false);
	}
	else
	{
		SetTriggerHeld(false);
		OptionsPanel = SNew(SSBTuningPanel)
			.Subsystem(USBTuningSubsystem::Get(this))
			.OnCloseRequested(FSimpleDelegate::CreateUObject(this, &ASBPlayerController::ToggleOptions));
		GEngine->GameViewport->AddViewportWidgetContent(OptionsPanel.ToSharedRef(), OptionsZOrder);
		SetPause(true);
	}
	RefreshInputMode();
}

void ASBPlayerController::HandleViewModeChanged(ESBViewMode NewMode)
{
	RefreshInputMode();
}

void ASBPlayerController::RefreshInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	if (OptionsPanel.IsValid())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(OptionsPanel);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
		return;
	}

	const ASBSnowmanCharacter* Snowman = GetPawn<ASBSnowmanCharacter>();
	const bool bTopDown = Snowman && Snowman->GetViewComponent()->GetViewMode() == ESBViewMode::TopDown;
	if (bTopDown)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(Mode);
		SetShowMouseCursor(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}
