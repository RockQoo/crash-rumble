#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CRGameSettings.generated.h"

class UMaterialInterface;

/**
 * 에셋 참조 모음 (프로젝트 설정 > Game > Crash Rumble).
 * 머티리얼 커스텀 프리미티브 데이터 규약: [0..3] Color(RGBA), [4] Flash/Intensity.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Crash Rumble"))
class CRASHRUMBLE_API UCRGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCRGameSettings();

	/** 눈사람/무기 기본 머티리얼. Color + Flash(피격 번쩍임). */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> CharacterMaterial;

	/** 발광 머티리얼 (빔, 탄, 총구 화염, 파티클). Color * Intensity. */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	/**
	 * 피격 파티클(인스턴스 메시)용 발광 머티리얼. 인스턴스 메시에는 Custom Primitive Data가 먹지 않아
	 * 일반 파라미터 Color / Intensity를 MID로 설정한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> ParticleMaterial;

	static UMaterialInterface* LoadCharacterMaterial();
	static UMaterialInterface* LoadGlowMaterial();
	static UMaterialInterface* LoadParticleMaterial();
};

namespace CRMaterialData
{
	constexpr int32 ColorIndex = 0;
	constexpr int32 FlashIndex = 4;
}
