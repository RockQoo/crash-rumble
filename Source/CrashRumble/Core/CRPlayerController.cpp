#include "Core/CRPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Camera/CRViewComponent.h"
#include "Character/CRCharacter.h"
#include "Core/CRTuningSubsystem.h"
#include "Core/SCRTuningPanel.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Styling/CoreStyle.h"
#include "Weapon/CRWeaponComponent.h"
#include "Core/CRActorPool.h"
#include "Core/CRTargetSpawner.h"
#include "Core/CRTuningSettings.h"
#include "Misc/App.h"
#include "TimerManager.h"
#include "Weapon/CRProjectile.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr int32 OptionsZOrder = 100;
	constexpr int32 HelpZOrder = 10;
}

FText ACRPlayerController::GetStatsText()
{
	// FPS는 지수 평활 (실시간 기준)
	const double Delta = FApp::GetDeltaTime();
	if (Delta > 0.0)
	{
		SmoothedFrameMs = SmoothedFrameMs <= 0.f ? Delta * 1000.0 : FMath::Lerp(SmoothedFrameMs, float(Delta * 1000.0), 0.05f);
	}

	const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this);
	const UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this);
	const int32 Monsters = Spawner ? Spawner->GetActiveCount() : 0;
	const int32 Bullets = Pool ? Pool->GetActiveCount(ACRProjectile::StaticClass()) : 0;
	const int32 BulletsIdle = Pool ? Pool->GetFreeCount(ACRProjectile::StaticClass()) : 0;
	const int32 Spawned = Pool ? Pool->GetTotalSpawned() : 0;

	FString Text = FString::Printf(
		TEXT("FPS %.0f (%.1f ms)\n몬스터 %d\n총알 비행 중 %d / 풀 대기 %d\n풀이 생성한 액터 누적 %d"),
		SmoothedFrameMs > 0.f ? 1000.f / SmoothedFrameMs : 0.f, SmoothedFrameMs, Monsters, Bullets, BulletsIdle, Spawned);
	if (Spawner && Spawner->IsBattleActive())
	{
		Text += FString::Printf(TEXT("\n\n레드  생존 %d · 처치 %d\n블루  생존 %d · 처치 %d"),
			Spawner->GetAliveCount(ECRTeam::Red), Spawner->GetKills(ECRTeam::Red),
			Spawner->GetAliveCount(ECRTeam::Blue), Spawner->GetKills(ECRTeam::Blue));
	}
	return FText::FromString(Text);
}

void ACRPlayerController::CRBattle(int32 PerTeam)
{
	if (!GetPawn())
	{
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this, PerTeam]() { CRBattle(PerTeam); }), 0.5f, false);
		return;
	}
	if (UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
	{
		Spawner->StartBattle(PerTeam);
	}
}

void ACRPlayerController::CRToggleView()
{
	OnToggleView();
}

void ACRPlayerController::CRDemo(float SecondsPerWeapon)
{
	DemoWeaponSeconds = FMath::Max(SecondsPerWeapon, 0.f);
	DemoTime = 0.f;
	DemoWeaponIndex = -1;
	if (DemoWeaponSeconds <= 0.f)
	{
		SetTriggerHeld(false);
		if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
		{
			Snowman->GetViewComponent()->ClearAimOverride();
		}
	}
}

void ACRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ACRCharacter* Snowman = GetPawn<ACRCharacter>();
	if (DemoWeaponSeconds <= 0.f || !Snowman || IsOptionsOpen())
	{
		return;
	}

	// 무기 순환: 칼 → 레이저 → 투사체
	DemoTime += DeltaTime;
	const int32 Index = FMath::FloorToInt(DemoTime / DemoWeaponSeconds) % 3;
	if (Index != DemoWeaponIndex)
	{
		DemoWeaponIndex = Index;
		static const ECRWeaponType Order[] = { ECRWeaponType::Sword, ECRWeaponType::Laser, ECRWeaponType::Projectile };
		Snowman->GetWeaponComponent()->Equip(Order[Index]);
	}

	// 가장 가까운 몬스터 조준: 숄더뷰는 카메라를 돌리고, 탑다운은 조준점을 직접 지정
	const UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this);
	const ACRCharacter* Target = Spawner ? Spawner->FindNearestTarget(Snowman->GetActorLocation()) : nullptr;
	if (!Target)
	{
		SetTriggerHeld(false);
		return;
	}
	const FVector TargetPoint = Target->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	if (Snowman->GetViewComponent()->GetViewMode() == ECRViewMode::TopDown)
	{
		Snowman->GetViewComponent()->SetAimOverride(TargetPoint);
	}
	else
	{
		Snowman->GetViewComponent()->ClearAimOverride();
		const FVector From = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : Snowman->GetActorLocation();
		SetControlRotation(FMath::RInterpTo(GetControlRotation(), (TargetPoint - From).Rotation(), DeltaTime, 12.f));
	}

	const float Distance = FVector::Dist2D(Snowman->GetActorLocation(), Target->GetActorLocation());
	const bool bSword = Snowman->GetWeaponComponent()->GetEquipped() == ECRWeaponType::Sword;
	SetTriggerHeld(!bSword || Distance < 220.f);
}

void ACRPlayerController::CRSpawn(int32 Count)
{
	// 시작 명령(-ExecCmds)처럼 플레이어가 생기기 전이면 잠시 뒤 다시 시도
	if (!GetPawn())
	{
		FTimerHandle Retry;
		GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this, Count]() { CRSpawn(Count); }), 0.5f, false);
		return;
	}
	if (UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
	{
		Spawner->SpawnTargets(Count);
	}
}

void ACRPlayerController::CRClear()
{
	if (UCRTargetSpawnerSubsystem* Spawner = UCRTargetSpawnerSubsystem::Get(this))
	{
		Spawner->ClearTargets();
	}
}

void ACRPlayerController::CRAutoFire(int32 Weapon, float Seconds)
{
	ACRCharacter* Snowman = GetPawn<ACRCharacter>();
	if (!Snowman)
	{
		return;
	}
	const ECRWeaponType Type = Weapon == 2 ? ECRWeaponType::Laser : Weapon == 3 ? ECRWeaponType::Projectile : ECRWeaponType::Sword;
	Snowman->GetWeaponComponent()->Equip(Type);
	SetTriggerHeld(true);
	GetWorldTimerManager().SetTimer(AutoFireTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SetTriggerHeld(false); }),
		FMath::Max(Seconds, 0.1f), false);
}

void ACRPlayerController::BeginPlay()
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
		TWeakObjectPtr<ACRPlayerController> WeakThis(this);
		HelpOverlay = SNew(SOverlay)
			// 왼쪽 위: 성능/개체 수
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(16.f)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetStatsText() : FText::GetEmpty(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 0.6f, 0.95f))
				.ShadowOffset(FVector2D(1.f, 1.f))
			]
			// 왼쪽 아래: 조작법
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(16.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("WASD 이동 · 마우스 시점 · 좌클릭 공격(꾹 누르면 연사) · 1 칼 / 2 레이저 / 3 투사체 · V 시점 전환 · Tab 튜닝 옵션 / 몬스터 생성")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
				.ShadowOffset(FVector2D(1.f, 1.f))
			];
		GEngine->GameViewport->AddViewportWidgetContent(HelpOverlay.ToSharedRef(), HelpZOrder);
	}

	RefreshInputMode();
}

void ACRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ACRPlayerController::CreateInputAssets()
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

void ACRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateInputAssets();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACRPlayerController::OnMove);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACRPlayerController::OnLook);
		Input->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ACRPlayerController::OnToggleView);
		Input->BindAction(OptionsAction, ETriggerEvent::Started, this, &ACRPlayerController::OnToggleOptions);
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &ACRPlayerController::OnFirePressed);
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ACRPlayerController::OnFireReleased);
		Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &ACRPlayerController::OnFireReleased);
		Input->BindAction(SwordAction, ETriggerEvent::Started, this, &ACRPlayerController::OnSelectSword);
		Input->BindAction(LaserAction, ETriggerEvent::Started, this, &ACRPlayerController::OnSelectLaser);
		Input->BindAction(ProjectileAction, ETriggerEvent::Started, this, &ACRPlayerController::OnSelectProjectile);
	}
}

void ACRPlayerController::SetTriggerHeld(bool bHeld)
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->GetWeaponComponent()->SetTriggerHeld(bHeld);
	}
}

void ACRPlayerController::OnFirePressed()
{
	SetTriggerHeld(true);
}

void ACRPlayerController::OnFireReleased()
{
	SetTriggerHeld(false);
}

void ACRPlayerController::OnSelectSword()
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ECRWeaponType::Sword);
	}
}

void ACRPlayerController::OnSelectLaser()
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ECRWeaponType::Laser);
	}
}

void ACRPlayerController::OnSelectProjectile()
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->GetWeaponComponent()->Equip(ECRWeaponType::Projectile);
	}
}

void ACRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ACRCharacter* Snowman = Cast<ACRCharacter>(InPawn))
	{
		Snowman->GetViewComponent()->OnViewModeChanged.AddUObject(this, &ACRPlayerController::HandleViewModeChanged);
	}
	RefreshInputMode();
}

void ACRPlayerController::OnMove(const FInputActionValue& Value)
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->MoveInput(Value.Get<FVector2D>());
	}
}

void ACRPlayerController::OnLook(const FInputActionValue& Value)
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->LookInput(Value.Get<FVector2D>());
	}
}

void ACRPlayerController::OnToggleView()
{
	if (ACRCharacter* Snowman = GetPawn<ACRCharacter>())
	{
		Snowman->ToggleViewMode();
	}
}

void ACRPlayerController::OnToggleOptions()
{
	ToggleOptions();
}

void ACRPlayerController::ToggleOptions()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	if (OptionsPanel.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OptionsPanel.ToSharedRef());
		OptionsPanel.Reset();
		if (UCRTuningSubsystem* Tuning = UCRTuningSubsystem::Get(this))
		{
			Tuning->Save();
		}
		SetPause(false);
	}
	else
	{
		SetTriggerHeld(false);
		TWeakObjectPtr<ACRPlayerController> WeakThis(this);
		OptionsPanel = SNew(SCRTuningPanel)
			.Subsystem(UCRTuningSubsystem::Get(this))
			.OnCloseRequested(FSimpleDelegate::CreateUObject(this, &ACRPlayerController::ToggleOptions))
			.OnSpawnTargets(FCROnSpawnTargets::CreateUObject(this, &ACRPlayerController::CRSpawn))
			.OnClearTargets(FSimpleDelegate::CreateUObject(this, &ACRPlayerController::CRClear))
			.OnStartBattle(FCROnSpawnTargets::CreateUObject(this, &ACRPlayerController::CRBattle))
			.StatusText_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetStatsText() : FText::GetEmpty(); });
		GEngine->GameViewport->AddViewportWidgetContent(OptionsPanel.ToSharedRef(), OptionsZOrder);
		// 기본은 게임이 계속 진행 → 바꾼 값이 바로 보인다
		SetPause(UCRTuningSubsystem::GetSettings(this).bPauseOnOptions);
	}
	RefreshInputMode();
}

void ACRPlayerController::HandleViewModeChanged(ECRViewMode NewMode)
{
	RefreshInputMode();
}

void ACRPlayerController::RefreshInputMode()
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

	const ACRCharacter* Snowman = GetPawn<ACRCharacter>();
	const bool bTopDown = Snowman && Snowman->GetViewComponent()->GetViewMode() == ECRViewMode::TopDown;
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
