#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CRCharacterAppearance.generated.h"

class UMaterialInterface;
class UStaticMesh;

/** 눈사람 파츠 하나. 위치는 캡슐 중심 기준. */
USTRUCT(BlueprintType)
struct FCRCharacterPartSpec
{
	GENERATED_BODY()

	/** 코드에서 찾는 이름 (HandL, HandR 등). */
	UPROPERTY(EditAnywhere, Category = "Part")
	FName Id;

	UPROPERTY(EditAnywhere, Category = "Part")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category = "Part")
	FTransform Transform;

	UPROPERTY(EditAnywhere, Category = "Part")
	FLinearColor Color = FLinearColor::White;

	/** true면 Color 대신 캐릭터의 AccentColor(모자/손 색)를 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Part")
	bool bUseAccentColor = false;
};

/**
 * 눈사람 외형 데이터. Tripo 모델 교체나 파츠 커스터마이즈는 이 에셋만 바꾸면 된다.
 * 캐릭터에 지정하지 않으면 MakeDefaultParts()의 기본 도형을 쓴다.
 */
UCLASS(BlueprintType)
class CRASHRUMBLE_API UCRCharacterAppearance : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TArray<FCRCharacterPartSpec> Parts;

	/** "Color" 벡터 파라미터를 가진 머티리얼. 비우면 엔진 BasicShapeMaterial. */
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TSoftObjectPtr<UMaterialInterface> Material;

	static const TArray<FCRCharacterPartSpec>& MakeDefaultParts();
};
