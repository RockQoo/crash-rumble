#include "Core/CRGameMode.h"

#include "Character/CRCharacter.h"
#include "Core/CRActorPool.h"
#include "Core/CRHUD.h"
#include "Core/CRPlayerController.h"
#include "Weapon/CRMissile.h"
#include "Weapon/CRProjectile.h"

namespace
{
	constexpr int32 ProjectilePrewarmCount = 32;
	constexpr int32 MissilePrewarmCount = 32;
}

ACRGameMode::ACRGameMode()
{
	DefaultPawnClass = ACRCharacter::StaticClass();
	PlayerControllerClass = ACRPlayerController::StaticClass();
	HUDClass = ACRHUD::StaticClass();
}

void ACRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 첫 발부터 스폰 없이 쏘도록 미리 채워 둔다
	if (UCRActorPoolSubsystem* Pool = UCRActorPoolSubsystem::Get(this))
	{
		Pool->Prewarm(ACRProjectile::StaticClass(), ProjectilePrewarmCount);
		Pool->Prewarm(ACRMissile::StaticClass(), MissilePrewarmCount);
	}
}
