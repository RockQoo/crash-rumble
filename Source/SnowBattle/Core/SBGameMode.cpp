#include "Core/SBGameMode.h"

#include "Character/SBSnowmanCharacter.h"
#include "Core/SBActorPool.h"
#include "Core/SBHUD.h"
#include "Core/SBPlayerController.h"
#include "Weapon/SBProjectile.h"

namespace
{
	constexpr int32 ProjectilePrewarmCount = 32;
}

ASBGameMode::ASBGameMode()
{
	DefaultPawnClass = ASBSnowmanCharacter::StaticClass();
	PlayerControllerClass = ASBPlayerController::StaticClass();
	HUDClass = ASBHUD::StaticClass();
}

void ASBGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 첫 발부터 스폰 없이 쏘도록 미리 채워 둔다
	if (USBActorPoolSubsystem* Pool = USBActorPoolSubsystem::Get(this))
	{
		Pool->Prewarm(ASBProjectile::StaticClass(), ProjectilePrewarmCount);
	}
}
