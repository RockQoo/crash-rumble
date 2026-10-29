#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CRTitlePlayerController.generated.h"

class ACameraActor;
class SCRTitleScreen;
enum class ECRMode : uint8;

/** 타이틀 화면 위젯을 띄우고, 맵에 놓인 캐릭터 주위로 배경 카메라를 돌린다. */
UCLASS()
class CRASHRUMBLE_API ACRTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACRTitlePlayerController();

	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleModeSelected(ECRMode Mode);
	void HandleQuit();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> OrbitCamera;

	TSharedPtr<SCRTitleScreen> Screen;
	FVector OrbitCenter = FVector::ZeroVector;
	float OrbitYaw = 0.f;
};
