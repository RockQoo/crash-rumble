#include "Feedback/CRShapeMeshes.h"

#include "Core/CRGameSettings.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"

namespace
{
	/** 면 단위 노멀로 삼각형을 쌓는 간단한 빌더 (꼭짓점 공유 없음 → 각진 툰 명암). */
	struct FFlatMesh
	{
		FMeshDescription Description;
		FMeshDescriptionBuilder Builder;
		FPolygonGroupID Group;

		FFlatMesh()
		{
			FStaticMeshAttributes Attributes(Description);
			Attributes.Register();
			Builder.SetMeshDescription(&Description);
			Builder.EnablePolyGroups();
			Builder.SetNumUVLayers(1);
			Group = Builder.AppendPolygonGroup(TEXT("Fx"));
		}

		FVertexInstanceID Instance(const FVector& Position, const FVector& Normal)
		{
			const FVertexID Vertex = Builder.AppendVertex(Position);
			const FVertexInstanceID Id = Builder.AppendInstance(Vertex);
			Builder.SetInstanceNormal(Id, Normal);
			Builder.SetInstanceUV(Id, FVector2D(Position.Y, Position.Z) / 100.f, 0);
			Builder.SetInstanceColor(Id, FVector4f(1.f, 1.f, 1.f, 1.f));
			return Id;
		}

		/** Normal을 바깥으로 보는 삼각형. 감김 방향을 노멀에 맞춘다. */
		void Triangle(const FVector& A, const FVector& B, const FVector& C, FVector Normal = FVector::ZeroVector)
		{
			const FVector Face = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				Normal = Face;
			}
			const bool bFlip = FVector::DotProduct(Face, Normal) > 0.f;
			const FVertexInstanceID I0 = Instance(A, Normal);
			const FVertexInstanceID I1 = Instance(bFlip ? C : B, Normal);
			const FVertexInstanceID I2 = Instance(bFlip ? B : C, Normal);
			Builder.AppendTriangle(I0, I1, I2, Group);
		}

		/** 앞뒤 두 면 (납작한 도형). */
		void DoubleTriangle(const FVector& A, const FVector& B, const FVector& C, const FVector& Normal)
		{
			Triangle(A, B, C, Normal);
			Triangle(A, B, C, -Normal);
		}

		UStaticMesh* Build(const TCHAR* Name)
		{
			UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), Name, RF_Transient);
			Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Fx"), TEXT("Fx")));
			UStaticMesh::FBuildMeshDescriptionsParams Params;
			Params.bBuildSimpleCollision = false;
			Params.bFastBuild = true;
			Params.bAllowCpuAccess = false;
			Mesh->BuildFromMeshDescriptions({ &Description }, Params);
			return Mesh;
		}
	};

	/** YZ 평면 별. Points개 꼭짓점, 바깥 반지름 50. */
	UStaticMesh* MakeStar(const TCHAR* Name, int32 Points, float InnerRatio)
	{
		FFlatMesh Mesh;
		const int32 Corners = Points * 2;
		for (int32 i = 0; i < Corners; ++i)
		{
			auto Corner = [Points, InnerRatio](int32 Index)
			{
				const float Angle = UE_PI * Index / Points + UE_HALF_PI;
				const float Radius = (Index % 2 == 0) ? 50.f : 50.f * InnerRatio;
				return FVector(0.f, FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
			};
			Mesh.DoubleTriangle(FVector::ZeroVector, Corner(i), Corner((i + 1) % Corners), FVector::ForwardVector);
		}
		return Mesh.Build(Name);
	}

	UStaticMesh* MakeSpike()
	{
		FFlatMesh Mesh;
		Mesh.DoubleTriangle(FVector(0.f, -50.f, 0.f), FVector(0.f, 50.f, 0.f), FVector(0.f, 0.f, 100.f), FVector::ForwardVector);
		return Mesh.Build(TEXT("CRFx_Spike"));
	}

	UStaticMesh* MakePyramid()
	{
		FFlatMesh Mesh;
		const FVector Apex(0.f, 0.f, 100.f);
		const FVector Corners[] = { FVector(50, 0, 0), FVector(0, 50, 0), FVector(-50, 0, 0), FVector(0, -50, 0) };
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector& A = Corners[i];
			const FVector& B = Corners[(i + 1) % 4];
			const FVector Outward = ((A + B) * 0.5f).GetSafeNormal();
			const FVector Normal = FVector::CrossProduct(B - A, Apex - A).GetSafeNormal();
			Mesh.Triangle(A, B, Apex, FVector::DotProduct(Normal, Outward) > 0.f ? Normal : -Normal);
		}
		Mesh.Triangle(Corners[0], Corners[1], Corners[2], FVector::DownVector);
		Mesh.Triangle(Corners[0], Corners[2], Corners[3], FVector::DownVector);
		return Mesh.Build(TEXT("CRFx_Pyramid"));
	}

	UStaticMesh* MakeDonut()
	{
		FFlatMesh Mesh;
		constexpr int32 Major = 24;
		constexpr int32 Minor = 10;
		constexpr float TubeRadius = 15.f;
		constexpr float RingRadius = 50.f - TubeRadius;
		auto Point = [](int32 I, int32 J, FVector& OutNormal)
		{
			const float U = UE_TWO_PI * I / Major;
			const float V = UE_TWO_PI * J / Minor;
			const FVector Axis(FMath::Cos(U), FMath::Sin(U), 0.f);
			OutNormal = Axis * FMath::Cos(V) + FVector::UpVector * FMath::Sin(V);
			return Axis * RingRadius + OutNormal * TubeRadius;
		};
		for (int32 i = 0; i < Major; ++i)
		{
			for (int32 j = 0; j < Minor; ++j)
			{
				FVector N00, N10, N01, N11;
				const FVector P00 = Point(i, j, N00);
				const FVector P10 = Point(i + 1, j, N10);
				const FVector P01 = Point(i, j + 1, N01);
				const FVector P11 = Point(i + 1, j + 1, N11);
				const FVector Normal = (N00 + N10 + N01 + N11).GetSafeNormal();
				Mesh.Triangle(P00, P10, P11, Normal);
				Mesh.Triangle(P00, P11, P01, Normal);
			}
		}
		return Mesh.Build(TEXT("CRFx_Donut"));
	}

	UStaticMesh* MakeShape(ECRFxShape Shape)
	{
		const UCRGameSettings* Settings = GetDefault<UCRGameSettings>();
		const TSoftObjectPtr<UStaticMesh>* Override = nullptr;
		switch (Shape)
		{
		case ECRFxShape::Cube:    return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		case ECRFxShape::Sphere:  return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		case ECRFxShape::Spike:   Override = &Settings->SpikeMesh; break;
		case ECRFxShape::Star4:   Override = &Settings->Star4Mesh; break;
		case ECRFxShape::Star8:   Override = &Settings->Star8Mesh; break;
		case ECRFxShape::Pyramid: Override = &Settings->PyramidMesh; break;
		case ECRFxShape::Donut:   Override = &Settings->DonutMesh; break;
		default: return nullptr;
		}
		if (Override && !Override->IsNull())
		{
			if (UStaticMesh* Asset = Override->LoadSynchronous())
			{
				return Asset;
			}
		}
		switch (Shape)
		{
		case ECRFxShape::Spike:   return MakeSpike();
		case ECRFxShape::Star4:   return MakeStar(TEXT("CRFx_Star4"), 4, 0.18f);
		case ECRFxShape::Star8:   return MakeStar(TEXT("CRFx_Star8"), 8, 0.45f);
		case ECRFxShape::Pyramid: return MakePyramid();
		case ECRFxShape::Donut:   return MakeDonut();
		default: return nullptr;
		}
	}
}

UStaticMesh* CRShapeMeshes::Get(ECRFxShape Shape)
{
	// 루트에 고정해 PIE를 여러 번 열어도 다시 만들지 않는다 (정적 소멸 순서 문제를 피하려고 스마트 포인터 대신 AddToRoot)
	static UStaticMesh* Cache[static_cast<int32>(ECRFxShape::Count)] = {};
	const int32 Index = static_cast<int32>(Shape);
	if (Index < 0 || Index >= static_cast<int32>(ECRFxShape::Count))
	{
		return nullptr;
	}
	if (!IsValid(Cache[Index]))
	{
		Cache[Index] = MakeShape(Shape);
		if (Cache[Index] && !Cache[Index]->IsRooted())
		{
			Cache[Index]->AddToRoot();
		}
	}
	return Cache[Index];
}
