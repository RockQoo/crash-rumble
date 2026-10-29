#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CRGameSettings.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMesh;
class UWorld;

/**
 * 에셋 참조 모음 (프로젝트 설정 > Game > Crash Rumble).
 * 머티리얼 커스텀 프리미티브 데이터 규약: [0..2] Color(RGB), [3] Flat(1 = 명암 없는 단색, 발광용), [4] Flash/Intensity, [5] 바닥 체크무늬.
 * 인스턴스 메시는 같은 번호의 Per Instance Custom Data를 쓴다 (툰 머티리얼이 둘 다 읽음).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Crash Rumble"))
class CRASHRUMBLE_API UCRGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCRGameSettings();

	/** 눈사람/무기 기본(조명) 머티리얼. 툰 셰이딩을 끄면 쓴다. Color + Flash(피격 번쩍임). */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> CharacterMaterial;

	/** 툰 머티리얼 (Unlit 2단 명암, 보라 그림자, 림). 캐릭터, 무기, 바닥, 이펙트 인스턴스 메시가 같이 쓴다. */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> ToonMaterial;

	/** 툰 머티리얼 공용 값 (빛 방향, 그림자 색, 림). 빛 방향은 레벨의 Directional Light에서 복사한다. */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialParameterCollection> ToonParameters;

	/** 블롭 그림자 (반투명 원판). 인스턴스 데이터 [0] = 진하기. */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> BlobShadowMaterial;

	/** 화면 연출 포스트 프로세스 (임팩트 프레임 / 집중선 / 슈퍼 프리즈). */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> ScreenFxMaterial;

	/** 발광 머티리얼 (빔, 탄, 총구 화염, 파티클). Color * Intensity. */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> GlowMaterial;

	/**
	 * 예전 피격 파티클(인스턴스 메시)용 발광 머티리얼. 색별 MID로 Color / Intensity를 설정한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> ParticleMaterial;

	/** 이펙트 도형. 비우면 코드로 만든 기본 도형 (가시: YZ 평면 삼각형, 밑변 중심이 원점, 끝 +Z 100). */
	UPROPERTY(config, EditAnywhere, Category = "Fx Shapes")
	TSoftObjectPtr<UStaticMesh> SpikeMesh;

	/** 4각 별 (YZ 평면, 반지름 50). */
	UPROPERTY(config, EditAnywhere, Category = "Fx Shapes")
	TSoftObjectPtr<UStaticMesh> Star4Mesh;

	/** 8각 별 (YZ 평면, 반지름 50). */
	UPROPERTY(config, EditAnywhere, Category = "Fx Shapes")
	TSoftObjectPtr<UStaticMesh> Star8Mesh;

	/** 사각뿔 (바닥 반지름 50, 높이 100, 피벗은 바닥). */
	UPROPERTY(config, EditAnywhere, Category = "Fx Shapes")
	TSoftObjectPtr<UStaticMesh> PyramidMesh;

	/** 도넛 (바깥 지름 100, 튜브 반지름 15, XY 평면). */
	UPROPERTY(config, EditAnywhere, Category = "Fx Shapes")
	TSoftObjectPtr<UStaticMesh> DonutMesh;

	/** 모드(타이틀 / 타격 체크 / 맵 제작 / 인게임)를 열 때 쓰는 맵. 모드는 게임모드로만 나뉜다. */
	UPROPERTY(config, EditAnywhere, Category = "Modes")
	TSoftObjectPtr<UWorld> ModeMap;

	static UMaterialInterface* LoadCharacterMaterial();
	static UMaterialInterface* LoadToonMaterial();
	static UMaterialInterface* LoadGlowMaterial();
	static UMaterialInterface* LoadParticleMaterial();

	/** 캐릭터/무기 표면 머티리얼: 툰 셰이딩 토글에 따라 툰 또는 조명 머티리얼. */
	static UMaterialInterface* LoadSurfaceMaterial(const UObject* WorldContext);
};

namespace CRMaterialData
{
	constexpr int32 ColorIndex = 0;
	constexpr int32 FlatIndex = 3;
	constexpr int32 FlashIndex = 4;
	constexpr int32 CheckerIndex = 5;

	/** 인스턴스 메시 커스텀 데이터 개수 (RGB + Flat). */
	constexpr int32 InstanceFloats = 4;
}
