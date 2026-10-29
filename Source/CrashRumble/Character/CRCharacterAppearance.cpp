#include "Character/CRCharacterAppearance.h"

#include "Engine/StaticMesh.h"

namespace
{
	const TCHAR* SnowmanSphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* SnowmanCylinderMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* SnowmanConeMesh = TEXT("/Engine/BasicShapes/Cone.Cone");

	const FLinearColor Snow(0.92f, 0.95f, 1.f);
	const FLinearColor Coal(0.02f, 0.02f, 0.025f);
	const FLinearColor Carrot(1.f, 0.35f, 0.05f);

	// 엔진 기본 도형은 지름 100 기준
	FCRCharacterPartSpec Part(const TCHAR* Id, const TCHAR* Mesh, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, bool bAccent = false, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FCRCharacterPartSpec Spec;
		Spec.Id = Id;
		Spec.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Mesh));
		Spec.Transform = FTransform(Rotation, Location, Scale);
		Spec.Color = Color;
		Spec.bUseAccentColor = bAccent;
		return Spec;
	}
}

const TArray<FCRCharacterPartSpec>& UCRCharacterAppearance::MakeDefaultParts()
{
	// 캡슐 반높이 100 기준: 바닥 z=-100
	static TArray<FCRCharacterPartSpec> Parts;
	if (Parts.Num() == 0)
	{
		Parts.Add(Part(TEXT("Body"), SnowmanSphereMesh, FVector(0, 0, -50), FVector(1.f), Snow));
		Parts.Add(Part(TEXT("Head"), SnowmanSphereMesh, FVector(0, 0, 25), FVector(0.66f), Snow));

		Parts.Add(Part(TEXT("HatBrim"), SnowmanCylinderMesh, FVector(0, 0, 58), FVector(0.55f, 0.55f, 0.04f), Coal, true));
		Parts.Add(Part(TEXT("HatCrown"), SnowmanCylinderMesh, FVector(0, 0, 77), FVector(0.38f, 0.38f, 0.36f), Coal, true));

		Parts.Add(Part(TEXT("EyeL"), SnowmanSphereMesh, FVector(29, -11, 34), FVector(0.08f), Coal));
		Parts.Add(Part(TEXT("EyeR"), SnowmanSphereMesh, FVector(29, 11, 34), FVector(0.08f), Coal));
		Parts.Add(Part(TEXT("Nose"), SnowmanConeMesh, FVector(42, 0, 25), FVector(0.1f, 0.1f, 0.28f), Carrot, false, FRotator(-90, 0, 0)));

		// 입: 웃는 모양(양 끝이 올라간 호)으로 놓은 숯 조각
		for (int32 i = 0; i < 5; ++i)
		{
			const float T = (i - 2) / 2.f;
			const FVector Location(31.f - FMath::Abs(T) * 3.f, T * 13.f, 13.f + T * T * 4.f);
			Parts.Add(Part(*FString::Printf(TEXT("Mouth%d"), i), SnowmanSphereMesh, Location, FVector(0.06f), Coal));
		}

		Parts.Add(Part(TEXT("ButtonTop"), SnowmanSphereMesh, FVector(48, 0, -30), FVector(0.07f), Coal));
		Parts.Add(Part(TEXT("ButtonBottom"), SnowmanSphereMesh, FVector(49, 0, -55), FVector(0.07f), Coal));

		Parts.Add(Part(TEXT("HandL"), SnowmanSphereMesh, FVector(15, -72, -35), FVector(0.22f), Snow, true));
		Parts.Add(Part(TEXT("HandR"), SnowmanSphereMesh, FVector(15, 72, -35), FVector(0.22f), Snow, true));
	}
	return Parts;
}
