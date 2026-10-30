#include "Core/CRInGameGameMode.h"

#include "Character/CRCharacter.h"
#include "Core/CRWaveSubsystem.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ACRInGameGameMode::ACRInGameGameMode()
{
	// 평평한 블럭 그리드 (파이고 부서지며 다시 자라지 않음)
	DefaultMapFile = TEXT("ingame");
}

void ACRInGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에 놓인 눈사람(훈련장 6m 허수아비)은 인게임에선 치운다 (패시브가 겨누지 않게). 플레이어 빙의가 끝난 다음 틱에
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		for (TActorIterator<ACRCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsPlayerControlled() && !It->IsInPool() && !It->IsDoppel())
			{
				It->Destroy();
			}
		}
	}));

	// 북쪽에서 몬스터가 계속 몰려오는 판 (플레이어가 쓰러지면 게임 오버)
	if (UCRWaveSubsystem* Wave = UCRWaveSubsystem::Get(this))
	{
		Wave->StartRun(true);
	}
}
