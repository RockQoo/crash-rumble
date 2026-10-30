#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/CRMapFile.h"
#include "CRBlockArena.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * 블럭 지형 + 구조물 (노션 블럭 지형 / 구조물 정리).
 * - 50cm 블럭 격자. 블럭 종류마다 인스턴스 메시 하나 (콜리전 BlockAll). 지면 맨 위가 월드 z = 0
 * - 칸마다 인스턴스 번호를 고정해 두고, 부서지면 인스턴스를 멀리 숨긴다 (번호 표가 깨지지 않는다)
 * - 붕괴: 블럭이 부서질 때 땅에 닿은 구조물에서 6방향 연결 검사(Flood Fill), 닿지 않은 블럭은 떨어진다
 * - 떨어지는 블럭: 콜리전 없는 인스턴스 메시, 착지는 격자 조회로 판단하고 큐브 잔해로 부서진다
 * - 재생: 6초 + 랜덤 + 층 × 0.15초 뒤 아래부터 다시 자람 (이웃이 있어야, 캐릭터가 있으면 미룸)
 * - 맵 파일(JSON)을 감시해 바뀌면 다시 불러온다 (맵 제작 모드 핫 리로드)
 */
UCLASS()
class CRASHRUMBLE_API ACRBlockArena : public AActor
{
	GENERATED_BODY()

public:
	ACRBlockArena();

	static ACRBlockArena* Find(const UObject* WorldContext);
	static ACRBlockArena* FromComponent(const UPrimitiveComponent* Component);

	/** 맵 파일을 불러와 아레나를 만든다 (없으면 스폰). 실패 시 nullptr, OutMessage에 이유. */
	static ACRBlockArena* LoadMap(UWorld* World, const FString& MapName, FString& OutMessage);
	/** 아레나를 지우고 원래 바닥을 되돌린다. */
	static void ClearMap(UWorld* World);

	bool Load(const FString& MapName, FString& OutMessage);

	/** Point(표면)의 블럭에 피해. 부서진 블럭 수. */
	int32 DamageAt(const FVector& Point, const FVector& Normal, int32 Damage);
	/** 반경 안 블럭을 모두 지운다 (크레이터). 부서진 블럭 수. */
	int32 DestroySphere(const FVector& Center, float Radius);
	/** 칸 좌표(x, y, 구조물 z = 지면 위 0층) 기준 DestroySphere. 시험·자동화용. */
	int32 DestroyCellSphere(const FIntVector& StructureCell, float Radius);

	int32 GetBlockCount() const { return SolidCount; }
	int32 GetFallingCount() const { return Falling.Num(); }
	const FString& GetMapName() const { return MapName; }
	const FString& GetStatusMessage() const { return StatusMessage; }
	/** 지면 맨 위 가운데 (월드). */
	FVector GetCenterTop() const;
	/** 맵 가로(X)·세로(Y) 절반 길이 (cm). */
	FVector2D GetHalfSize() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FFallingBlock
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		float SpinSpeed = 0.f;
		ECRBlock Type = ECRBlock::Empty;
	};

	struct FRegrow
	{
		int32 Cell = INDEX_NONE;
		float Age = 0.f;
	};

	void Rebuild(FCRMapSpec&& Spec);
	/** 맵 파일 objects를 칸 맨 위에 세운다 (다시 불러오면 지우고 새로). */
	void SpawnObjects(const TArray<FCRMapObjectSpec>& Objects);
	void ClearObjects();
	/** 그 칸 기둥에서 가장 높은 블럭 윗면 (월드 z). 블럭이 없으면 지면 높이. */
	float GetColumnTopZ(int32 X, int32 Y) const;
	void SetFloorsHidden(bool bHideFloors);
	void CheckFileChanged(float DeltaSeconds);

	int32 CellAt(const FVector& WorldPoint) const;
	FVector CellCenter(int32 Cell) const;
	FIntVector CellCoord(int32 Cell) const;
	bool IsSolid(int32 Cell) const { return Cells.IsValidIndex(Cell) && Cells[Cell] != 0; }
	bool HasSolidNeighbor(int32 Cell) const;
	FVector4f GetCellColor(int32 Cell, ECRBlock Type, float Brightness = 1.f) const;
	FTransform GetCellTransform(int32 Cell, float Scale = 1.f) const;

	void RemoveCell(int32 Cell, bool bDebris, const FVector& Normal);
	void ScheduleRegen(int32 Cell);
	void ShowCell(int32 Cell, float Scale, float Brightness);
	void HideCell(int32 Cell);
	/** 땅에 닿지 않은 구조물 블럭을 떨어뜨린다. 떨어진 수. */
	int32 CollapseUnsupported();
	/** 붕괴 검사를 이번 프레임 끝(Tick)에 한 번만 하도록 예약. */
	void RequestCollapse();

	void UpdateFalling(float DeltaSeconds);
	void UpdateRegen(float DeltaSeconds);
	void UpdateCrystalSparkle(float DeltaSeconds);
	void FlushFalling();
	void MarkDirty(ECRBlock Type) { DirtyTypes |= (1 << static_cast<int32>(Type)); }
	void FlushDirty();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TypeComponents;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FallingComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> HiddenFloors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedObjects;

	FString MapName;
	FString MapPath;
	FDateTime MapTimestamp;
	/** 맵 파일 "regen" (false면 지형 재생 안 함). */
	bool bMapRegen = true;
	float FileCheckTimer = 0.f;
	FString StatusMessage;

	int32 SizeX = 0;
	int32 SizeY = 0;
	int32 SizeZ = 0;
	int32 GroundLayers = 0;
	FVector Origin = FVector::ZeroVector; // 칸 (0,0,0)의 최소 모서리

	TArray<uint8> Cells;    // 지금 블럭 (0 = 빈칸)
	TArray<uint8> Original; // 원래 블럭
	TArray<uint8> Health;
	TArray<int32> Instance; // 원래 블럭 종류 인스턴스 메시 안의 고정 번호
	TArray<float> RegenAt;  // 월드 시간, 0 = 재생 대기 아님
	TArray<int32> RegenQueue;
	/** 재생 큐를 프레임마다 조금씩 이어서 도는 위치 (뒤에서 앞으로). */
	int32 RegenCursor = 0;
	TArray<FRegrow> Regrowing;
	TArray<FFallingBlock> Falling;
	int32 FallingDrawn = 0;
	int32 SolidCount = 0;
	uint32 DirtyTypes = 0;
	/** 이번 프레임에 블럭이 부서짐 → Tick에서 붕괴 검사 한 번. */
	bool bCollapsePending = false;
	float SparkleTimer = 0.f;
};
