#pragma once

#include "CoreMinimal.h"
#include "Feedback/CRShapeMeshes.h"
#include "Subsystems/WorldSubsystem.h"
#include "CRFxSubsystem.generated.h"

class UInstancedStaticMeshComponent;

/**
 * 도형 애니 이펙트 (노션 연출 1·2차).
 * - 2코마 시계: 계산은 매 프레임, 그리기는 FxFps(기본 12)마다 한 번. 캐릭터 2코마(CharFps)도 여기서 판단한다.
 * - 이펙트 조각은 도형별 인스턴스 메시 하나에 모아 그린다 (툰 머티리얼 + Per Instance Custom Data 색). 조각 버퍼는 고정 크기로 재사용.
 * - 블롭 그림자: 캐릭터가 매 프레임 AddBlobShadow로 요청 → 인스턴스 메시 하나로 그린다.
 * 색 값은 1보다 크게 주면 블룸이 걸려 발광처럼 보인다 (Flat = 명암 없음).
 */
UCLASS()
class CRASHRUMBLE_API UCRFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCRFxSubsystem* Get(const UObject* WorldContext);

	/** 이번 프레임에 이펙트를 다시 그리는가 (2코마). 서브시스템이 없으면 true. */
	static bool IsFxDrawFrame(const UObject* WorldContext);
	bool IsFxDrawFrame() const { return bFxDrawFrame; }
	/** 캐릭터 2코마: 이번 프레임에 캐릭터 모습을 화면에 반영하는가. */
	bool IsCharDrawFrame() const { return bCharDrawFrame; }
	bool IsCharSteppingEnabled() const;
	int32 GetFxDrawCount() const { return FxDrawCount; }

	// 블롭 그림자 (매 프레임 요청)
	void AddBlobShadow(const FVector& GroundPoint, float Radius, float Strength);

	// 이펙트
	/** 유타폰 큐브: 표면 안쪽에서 튀어나와 날아가고, 튕기고, 작아져 사라진다. 색은 A/B를 섞는다. */
	void SpawnDebris(const FVector& Location, const FVector& Normal, int32 Count, const FLinearColor& ColorA, const FLinearColor& ColorB,
		float SizeScale = 1.f, float GroundZ = -UE_BIG_NUMBER);
	/** 구체 폭발: 섬광 2장 + 불덩이 10개(단계 색) + 바닥 충격파 큐브 링. */
	void SpawnSphereExplosion(const FVector& Center, float Radius, float GroundZ);
	/** 히트 스파크 별 (8각 별 3장). Size = 반지름 cm. */
	void SpawnHitStar(const FVector& Location, float Size, const FLinearColor& Color);
	/** 십자 반짝임. Follow가 있으면 그 컴포넌트를 따라간다. bRealTime이면 시간 정지 중에도 움직인다. */
	void SpawnSparkle(const FVector& Location, float Size, USceneComponent* Follow = nullptr, bool bRealTime = false);
	/** 카나다 빛: 카메라 평면에 방사하는 가시 12개. */
	void SpawnKanada(const FVector& Location, float Size);
	/** 착지 먼지 링: 납작한 도넛이 퍼지고 먼지 구체 8개가 밀려난다. */
	void SpawnDustRing(const FVector& GroundLocation, float Size);
	/** 지그재그 번개 (그릴 때마다 모양 재생성). */
	void SpawnLightning(const FVector& Start, const FVector& End, float Duration = 0.32f);
	/** 폴리곤 불꽃: 삼각뿔 22개, 장마다 재추첨. */
	void SpawnFlame(const FVector& GroundCenter, float Radius, float Height, float Duration);
	/** 미사일 연기 한 덩이. bRing이면 진행 방향에 수직인 도넛 링도. */
	void SpawnSmokePuff(const FVector& Location, float Size, const FVector& Direction, bool bRing, float Lifetime = -1.f);
	/** 스미어: 지난 그림과 이번 그림 사이를 늘린 구체로 채운다 (다음 그림에서 사라짐). */
	void SpawnSmear(const FVector& From, const FVector& To, float Diameter, const FLinearColor& Color);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	enum class EPieceKind : uint8
	{
		Debris,
		Fireball,
		Flash,
		RingCube,
		Smoke,
		SmokeRing,
		DustPuff,
		DustRing,
		Smear,
	};

	struct FPiece
	{
		EPieceKind Kind = EPieceKind::Debris;
		ECRFxShape Shape = ECRFxShape::Cube;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Origin = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinSpeed = 0.f; // 라디안/초
		FVector Scale = FVector::OneVector;
		float Size = 1.f;
		float Age = 0.f;
		float Lifetime = 1.f;
		float Delay = 0.f;
		float GroundZ = -UE_BIG_NUMBER;
		FLinearColor Color = FLinearColor::White;
		float Flat = 0.f;
		bool bAlive = false;
	};

	enum class EProcKind : uint8
	{
		HitStar,
		Sparkle,
		Kanada,
		Lightning,
		Flame,
	};

	/** 그릴 때마다 모양을 새로 만드는 이펙트. */
	struct FProc
	{
		EProcKind Kind = EProcKind::HitStar;
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		float Size = 1.f;
		float Height = 0.f;
		float Age = 0.f;
		float Lifetime = 0.3f;
		FLinearColor Color = FLinearColor::White;
		int32 Seed = 0;
		bool bRealTime = false;
		bool bAlive = false;
		TWeakObjectPtr<USceneComponent> Follow;
		FVector FollowOffset = FVector::ZeroVector;
	};

	struct FDrawList
	{
		TArray<FTransform> Transforms;
		TArray<float> Custom; // 인스턴스당 InstanceFloats개
		int32 LastUsed = 0;
	};

	void HandleWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	FPiece& NextPiece();
	FProc& NextProc();
	void UpdatePieces(float DeltaTime, float RealDeltaTime);
	void UpdateProcs(float DeltaTime, float RealDeltaTime);
	void DrawPieces();
	void DrawProcs();
	void DrawProc(const FProc& Proc, FRandomStream& Random);
	void Emit(ECRFxShape Shape, const FTransform& Transform, const FLinearColor& Color, float Flat);
	void FlushShapes();
	void FlushBlobs();
	bool EnsureComponents();
	UInstancedStaticMeshComponent* MakeComponent(UStaticMesh* Mesh, UMaterialInterface* Material, int32 NumCustomFloats);
	FQuat FaceCamera(const FVector& Location, float Roll) const;
	bool HasRealTimeEffects() const;

	UPROPERTY()
	TObjectPtr<AActor> FxActor;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ShapeComponents;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BlobComponent;

	TArray<FDrawList> ShapeLists;
	FDrawList BlobList;

	TArray<FPiece> Pieces;
	int32 NextPieceIndex = 0;
	int32 AlivePieces = 0;
	TArray<FProc> Procs;
	int32 NextProcIndex = 0;
	int32 AliveProcs = 0;
	bool bShapesDirty = false;

	// 2코마 시계
	FDelegateHandle TickStartHandle;
	float FxAccumulator = 0.f;
	float CharAccumulator = 0.f;
	bool bFxDrawFrame = true;
	bool bCharDrawFrame = true;
	int32 FxDrawCount = 0;

	FVector CameraLocation = FVector::ZeroVector;
	FQuat CameraRotation = FQuat::Identity;
	int32 SeedCounter = 1;
};
