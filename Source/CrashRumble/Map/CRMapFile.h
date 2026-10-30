#pragma once

#include "CoreMinimal.h"

/** 블럭 종류. 0은 빈칸. */
enum class ECRBlock : uint8
{
	Empty,
	Grass,
	Dirt,
	Stone,
	Brick,
	Wood,
	Crystal,
	Count
};

/** 훈련장 오브젝트 종류. */
enum class ECRMapObject : uint8
{
	Dummy,        // 허수아비: 죽으면 제자리에서 다시 일어남
	DpsDummy,     // DPS 허수아비: 죽지 않고 머리 위에 초당/누적 데미지
	MovingTarget, // from ↔ to 왕복하는 허수아비
	Barrel,       // 폭발 드럼통: 맞으면 터져 주변 피해 + 크레이터, 연쇄 폭발, 다시 생김
};

/** 블럭 위에 놓는 오브젝트. 높이는 그 칸의 가장 높은 블럭 위. */
struct FCRMapObjectSpec
{
	ECRMapObject Type = ECRMapObject::Dummy;
	FIntPoint At = FIntPoint::ZeroValue;
	FIntPoint To = FIntPoint::ZeroValue;
	float Speed = 200.f;
};

/** 맵 파일을 펼친 결과: 칸마다 원래 블럭 종류. */
struct FCRMapSpec
{
	FString Name;
	int32 SizeX = 64;
	int32 SizeY = 64;
	int32 GroundLayers = 4;     // 지면 층 수 (맨 위가 z = GroundLayers - 1)
	int32 StructureLayers = 14; // 지면 위 구조물 공간
	bool bRegen = true;          // "regen": false면 부서진 블럭이 다시 자라지 않음 (인게임)
	TArray<uint8> Cells;        // x + y * SizeX + z * SizeX * SizeY
	TArray<FCRMapObjectSpec> Objects;
	TArray<FString> Warnings;   // 무시한 명령 (AI가 고칠 수 있게 몇 번째 명령인지 적는다)

	int32 GetSizeZ() const { return GroundLayers + StructureLayers; }
	int32 Index(int32 X, int32 Y, int32 Z) const { return X + Y * SizeX + Z * SizeX * SizeY; }
	bool IsInside(int32 X, int32 Y, int32 Z) const
	{
		return X >= 0 && Y >= 0 && Z >= 0 && X < SizeX && Y < SizeY && Z < GetSizeZ();
	}
};

/**
 * AI가 쓰기 쉬운 맵 파일(JSON). 블럭을 하나씩 찍지 않고 구조물 단위 명령으로 쓴다.
 * 형식: docs/맵-파일-형식.md, 예시: Content/MapData/arena.json
 */
namespace CRMapFile
{
	/** Content/MapData/<Name>.json (Name에 확장자가 있거나 절대 경로면 그대로). */
	CRASHRUMBLE_API FString ResolvePath(const FString& Name);
	CRASHRUMBLE_API bool LoadFile(const FString& Name, FCRMapSpec& OutSpec, FString& OutError);
	CRASHRUMBLE_API bool Parse(const FString& Json, FCRMapSpec& OutSpec, FString& OutError);

	CRASHRUMBLE_API const TCHAR* GetBlockName(ECRBlock Block);
	CRASHRUMBLE_API ECRBlock FindBlock(const FString& Name);
	CRASHRUMBLE_API int32 GetBlockHealth(ECRBlock Block);
	CRASHRUMBLE_API FLinearColor GetBlockColor(ECRBlock Block);
	/** 발광 블럭(크리스탈)은 명암 없이 밝게. */
	CRASHRUMBLE_API bool IsGlowBlock(ECRBlock Block);
	/** GameData/blocks.json: 블럭 HP(1~255)·색 덮어쓰기. 새로 불러오는 맵부터 적용. */
	CRASHRUMBLE_API void SetBlockOverride(ECRBlock Block, int32 Health, const FLinearColor& Color);
	CRASHRUMBLE_API void ResetBlockOverrides();
}
