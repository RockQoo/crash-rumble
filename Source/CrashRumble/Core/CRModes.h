#pragma once

#include "CoreMinimal.h"

/** 게임 모드 종류. 모두 같은 맵(UCRGameSettings::ModeMap)을 쓰고 게임모드만 바꿔 연다. */
enum class ECRMode : uint8
{
	Title,
	HitCheck,
	MapEdit,
	InGame,
};

/**
 * 모드 전환. 맵 URL 옵션 `game=<별칭>`으로 게임모드를 고른다 (별칭은 DefaultEngine.ini GameModeClassAliases).
 * 콘솔/시작 명령: `CRMode Title | HitCheck | MapEdit | InGame`
 */
namespace CRModes
{
	CRASHRUMBLE_API void Open(const UObject* WorldContext, ECRMode Mode);
	CRASHRUMBLE_API ECRMode GetCurrent(const UObject* WorldContext);
	CRASHRUMBLE_API FText GetDisplayName(ECRMode Mode);
	CRASHRUMBLE_API const TCHAR* GetAlias(ECRMode Mode);
}
