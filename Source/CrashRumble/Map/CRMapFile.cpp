#include "Map/CRMapFile.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	struct FBlockInfo
	{
		const TCHAR* Name;
		int32 Health;
		FLinearColor Color;
	};

	// 잔디(HP 2), 흙(2), 돌(4), 벽돌(3), 나무(2), 크리스탈(1, 발광)
	const FBlockInfo Blocks[] = {
		{ TEXT("empty"), 0, FLinearColor::Black },
		{ TEXT("grass"), 2, FLinearColor(0.36f, 0.68f, 0.26f) },
		{ TEXT("dirt"), 2, FLinearColor(0.52f, 0.34f, 0.2f) },
		{ TEXT("stone"), 4, FLinearColor(0.52f, 0.53f, 0.58f) },
		{ TEXT("brick"), 3, FLinearColor(0.72f, 0.3f, 0.22f) },
		{ TEXT("wood"), 2, FLinearColor(0.62f, 0.43f, 0.22f) },
		{ TEXT("crystal"), 1, FLinearColor(0.5f, 2.2f, 3.f) },
	};
	static_assert(UE_ARRAY_COUNT(Blocks) == static_cast<int32>(ECRBlock::Count), "블럭 표와 enum 개수가 같아야 함");

	/** 명령 하나를 펼치는 동안 쓰는 도우미. 구조물 z는 지면 바로 위가 0. */
	struct FBuilder
	{
		FCRMapSpec& Spec;

		void Set(int32 X, int32 Y, int32 StructureZ, ECRBlock Block)
		{
			const int32 Z = StructureZ + Spec.GroundLayers;
			if (Spec.IsInside(X, Y, Z))
			{
				Spec.Cells[Spec.Index(X, Y, Z)] = static_cast<uint8>(Block);
			}
		}

		/** 지면 층 직접 (0 = 지면 맨 위층, 1 = 그 아래 ...). */
		void SetGround(int32 X, int32 Y, int32 Depth, ECRBlock Block)
		{
			const int32 Z = Spec.GroundLayers - 1 - Depth;
			if (Spec.IsInside(X, Y, Z) && Z >= 0)
			{
				Spec.Cells[Spec.Index(X, Y, Z)] = static_cast<uint8>(Block);
			}
		}
	};

	bool ReadInt2(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FIntPoint& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values->Num() < 2)
		{
			return false;
		}
		Out = FIntPoint(FMath::RoundToInt((*Values)[0]->AsNumber()), FMath::RoundToInt((*Values)[1]->AsNumber()));
		return true;
	}

	bool ReadInt3(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FIntVector& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values->Num() < 2)
		{
			return false;
		}
		Out = FIntVector(FMath::RoundToInt((*Values)[0]->AsNumber()), FMath::RoundToInt((*Values)[1]->AsNumber()),
			Values->Num() > 2 ? FMath::RoundToInt((*Values)[2]->AsNumber()) : 0);
		return true;
	}

	int32 ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default)
	{
		double Value = Default;
		return Object->TryGetNumberField(Field, Value) ? FMath::RoundToInt(Value) : Default;
	}

	bool ReadBool(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool bDefault)
	{
		bool bValue = bDefault;
		return Object->TryGetBoolField(Field, bValue) ? bValue : bDefault;
	}

	/** from → to 직선 위의 칸 (축 방향이 아니어도 계단처럼 이어진다). */
	TArray<FIntPoint> LineCells(const FIntPoint& From, const FIntPoint& To)
	{
		TArray<FIntPoint> Result;
		const int32 Steps = FMath::Max(FMath::Abs(To.X - From.X), FMath::Abs(To.Y - From.Y));
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float T = Steps > 0 ? float(i) / Steps : 0.f;
			Result.AddUnique(FIntPoint(FMath::RoundToInt(FMath::Lerp(float(From.X), float(To.X), T)),
				FMath::RoundToInt(FMath::Lerp(float(From.Y), float(To.Y), T))));
		}
		return Result;
	}

	FString Describe(int32 Index, const FString& Type)
	{
		return FString::Printf(TEXT("structures[%d] (%s)"), Index, *Type);
	}
}

const TCHAR* CRMapFile::GetBlockName(ECRBlock Block)
{
	const int32 Index = static_cast<int32>(Block);
	return Index >= 0 && Index < UE_ARRAY_COUNT(Blocks) ? Blocks[Index].Name : TEXT("?");
}

ECRBlock CRMapFile::FindBlock(const FString& Name)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(Blocks); ++i)
	{
		if (Name.Equals(Blocks[i].Name, ESearchCase::IgnoreCase))
		{
			return static_cast<ECRBlock>(i);
		}
	}
	return ECRBlock::Count;
}

int32 CRMapFile::GetBlockHealth(ECRBlock Block)
{
	const int32 Index = static_cast<int32>(Block);
	return Index > 0 && Index < UE_ARRAY_COUNT(Blocks) ? Blocks[Index].Health : 1;
}

FLinearColor CRMapFile::GetBlockColor(ECRBlock Block)
{
	const int32 Index = static_cast<int32>(Block);
	return Index >= 0 && Index < UE_ARRAY_COUNT(Blocks) ? Blocks[Index].Color : FLinearColor::White;
}

bool CRMapFile::IsGlowBlock(ECRBlock Block)
{
	return Block == ECRBlock::Crystal;
}

FString CRMapFile::ResolvePath(const FString& Name)
{
	if (FPaths::IsRelative(Name) == false)
	{
		return Name;
	}
	const FString File = Name.EndsWith(TEXT(".json")) ? Name : Name + TEXT(".json");
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MapData"), File);
}

bool CRMapFile::LoadFile(const FString& Name, FCRMapSpec& OutSpec, FString& OutError)
{
	const FString Path = ResolvePath(Name);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		OutError = FString::Printf(TEXT("맵 파일을 열 수 없음: %s"), *Path);
		return false;
	}
	if (!Parse(Text, OutSpec, OutError))
	{
		return false;
	}
	if (OutSpec.Name.IsEmpty())
	{
		OutSpec.Name = FPaths::GetBaseFilename(Path);
	}
	return true;
}

bool CRMapFile::Parse(const FString& Json, FCRMapSpec& OutSpec, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("JSON 문법 오류: %s"), *Reader->GetErrorMessage());
		return false;
	}

	FCRMapSpec Spec;
	Root->TryGetStringField(TEXT("name"), Spec.Name);
	FIntPoint Size(64, 64);
	ReadInt2(Root, TEXT("size"), Size);
	Spec.SizeX = FMath::Clamp(Size.X, 4, 256);
	Spec.SizeY = FMath::Clamp(Size.Y, 4, 256);
	Spec.StructureLayers = FMath::Clamp(ReadInt(Root, TEXT("height"), 14), 1, 64);

	// 지면: 위에서 아래 순서의 블럭 이름 (기본 잔디 / 흙 / 흙 / 돌)
	TArray<ECRBlock> Ground = { ECRBlock::Grass, ECRBlock::Dirt, ECRBlock::Dirt, ECRBlock::Stone };
	const TArray<TSharedPtr<FJsonValue>>* GroundValues = nullptr;
	if (Root->TryGetArrayField(TEXT("ground"), GroundValues) && GroundValues->Num() > 0)
	{
		Ground.Reset();
		for (int32 i = 0; i < GroundValues->Num(); ++i)
		{
			const ECRBlock Block = FindBlock((*GroundValues)[i]->AsString());
			if (Block == ECRBlock::Count)
			{
				Spec.Warnings.Add(FString::Printf(TEXT("ground[%d]: 모르는 블럭 '%s' → dirt"), i, *(*GroundValues)[i]->AsString()));
			}
			Ground.Add(Block == ECRBlock::Count ? ECRBlock::Dirt : Block);
		}
	}
	Spec.GroundLayers = FMath::Clamp(Ground.Num(), 1, 16);
	Spec.Cells.SetNumZeroed(Spec.SizeX * Spec.SizeY * Spec.GetSizeZ());
	for (int32 Depth = 0; Depth < Spec.GroundLayers; ++Depth)
	{
		const int32 Z = Spec.GroundLayers - 1 - Depth;
		for (int32 Y = 0; Y < Spec.SizeY; ++Y)
		{
			for (int32 X = 0; X < Spec.SizeX; ++X)
			{
				Spec.Cells[Spec.Index(X, Y, Z)] = static_cast<uint8>(Ground[Depth]);
			}
		}
	}

	FBuilder Build{ Spec };
	const TArray<TSharedPtr<FJsonValue>>* Structures = nullptr;
	if (Root->TryGetArrayField(TEXT("structures"), Structures))
	{
		for (int32 i = 0; i < Structures->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> Item = (*Structures)[i]->AsObject();
			if (!Item.IsValid())
			{
				Spec.Warnings.Add(FString::Printf(TEXT("structures[%d]: 객체가 아님"), i));
				continue;
			}
			FString Type;
			Item->TryGetStringField(TEXT("type"), Type);
			Type = Type.ToLower();
			FString BlockName = TEXT("stone");
			Item->TryGetStringField(TEXT("block"), BlockName);
			ECRBlock Block = FindBlock(BlockName);
			if (Block == ECRBlock::Count || Block == ECRBlock::Empty)
			{
				Spec.Warnings.Add(FString::Printf(TEXT("%s: 모르는 블럭 '%s' → stone"), *Describe(i, Type), *BlockName));
				Block = ECRBlock::Stone;
			}

			FIntPoint At, From, To;
			if (Type == TEXT("wall"))
			{
				// 성벽: from → to, 높이, 두께, 성문(가운데 기준 칸 수), 총안(꼭대기 한 칸 걸러)
				if (!ReadInt2(Item, TEXT("from"), From) || !ReadInt2(Item, TEXT("to"), To))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": from / to 필요"));
					continue;
				}
				const int32 Height = ReadInt(Item, TEXT("height"), 5);
				const int32 Thickness = FMath::Max(1, ReadInt(Item, TEXT("thickness"), 1));
				const TArray<FIntPoint> Line = LineCells(From, To);
				const bool bAlongX = FMath::Abs(To.X - From.X) >= FMath::Abs(To.Y - From.Y);
				int32 GateCenter = -1, GateWidth = 0, GateHeight = 0;
				const TSharedPtr<FJsonObject>* Gate = nullptr;
				if (Item->TryGetObjectField(TEXT("gate"), Gate))
				{
					GateCenter = ReadInt(*Gate, TEXT("at"), Line.Num() / 2);
					GateWidth = ReadInt(*Gate, TEXT("width"), 3);
					GateHeight = ReadInt(*Gate, TEXT("height"), FMath::Max(1, Height - 2));
				}
				const bool bBattlements = ReadBool(Item, TEXT("battlements"), false);
				for (int32 Step = 0; Step < Line.Num(); ++Step)
				{
					const bool bInGate = GateCenter >= 0 && FMath::Abs(Step - GateCenter) <= GateWidth / 2;
					for (int32 T = 0; T < Thickness; ++T)
					{
						const int32 X = Line[Step].X + (bAlongX ? 0 : T);
						const int32 Y = Line[Step].Y + (bAlongX ? T : 0);
						for (int32 Z = bInGate ? GateHeight : 0; Z < Height; ++Z)
						{
							Build.Set(X, Y, Z, Block);
						}
						if (bBattlements && Step % 2 == 0)
						{
							Build.Set(X, Y, Height, Block);
						}
					}
				}
			}
			else if (Type == TEXT("tower"))
			{
				// 탑: 원형, 속 빈 벽(기본), 꼭대기 총안
				if (!ReadInt2(Item, TEXT("at"), At))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": at 필요"));
					continue;
				}
				const int32 Radius = FMath::Max(1, ReadInt(Item, TEXT("radius"), 3));
				const int32 Height = ReadInt(Item, TEXT("height"), 10);
				const bool bHollow = ReadBool(Item, TEXT("hollow"), true);
				const bool bDoor = ReadBool(Item, TEXT("door"), true);
				for (int32 Dy = -Radius; Dy <= Radius; ++Dy)
				{
					for (int32 Dx = -Radius; Dx <= Radius; ++Dx)
					{
						const float Distance = FMath::Sqrt(float(Dx * Dx + Dy * Dy));
						if (Distance > Radius + 0.4f || (bHollow && Distance < Radius - 0.6f))
						{
							continue;
						}
						const bool bRim = Distance >= Radius - 0.6f;
						for (int32 Z = 0; Z < Height; ++Z)
						{
							// 문: +X 쪽 바닥 2칸
							if (bDoor && Dy == 0 && Dx > 0 && bRim && Z < 2)
							{
								continue;
							}
							Build.Set(At.X + Dx, At.Y + Dy, Z, Block);
						}
						if (bRim && (Dx + Dy) % 2 == 0)
						{
							Build.Set(At.X + Dx, At.Y + Dy, Height, Block);
						}
					}
				}
			}
			else if (Type == TEXT("pyramid"))
			{
				// 계단식 피라미드: at = 중심, base = 밑변 칸 수
				if (!ReadInt2(Item, TEXT("at"), At))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": at 필요"));
					continue;
				}
				const int32 Base = FMath::Max(1, ReadInt(Item, TEXT("base"), 9));
				for (int32 Z = 0; ; ++Z)
				{
					const int32 Half = (Base - 1) / 2 - Z;
					if (Half < 0)
					{
						break;
					}
					for (int32 Dy = -Half; Dy <= Half; ++Dy)
					{
						for (int32 Dx = -Half; Dx <= Half; ++Dx)
						{
							Build.Set(At.X + Dx, At.Y + Dy, Z, Block);
						}
					}
				}
			}
			else if (Type == TEXT("bridge"))
			{
				// 다리: from → to 상판 (height 층), 폭, 양 끝 기둥
				if (!ReadInt2(Item, TEXT("from"), From) || !ReadInt2(Item, TEXT("to"), To))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": from / to 필요"));
					continue;
				}
				const int32 Deck = FMath::Max(1, ReadInt(Item, TEXT("height"), 4)) - 1;
				const int32 Width = FMath::Max(1, ReadInt(Item, TEXT("width"), 2));
				const bool bAlongX = FMath::Abs(To.X - From.X) >= FMath::Abs(To.Y - From.Y);
				const TArray<FIntPoint> Line = LineCells(From, To);
				for (const FIntPoint& Point : Line)
				{
					for (int32 W = 0; W < Width; ++W)
					{
						Build.Set(Point.X + (bAlongX ? 0 : W), Point.Y + (bAlongX ? W : 0), Deck, Block);
					}
				}
				for (const FIntPoint& End : { From, To })
				{
					for (int32 W = 0; W < Width; ++W)
					{
						for (int32 Z = 0; Z < Deck; ++Z)
						{
							Build.Set(End.X + (bAlongX ? 0 : W), End.Y + (bAlongX ? W : 0), Z, Block);
						}
					}
				}
			}
			else if (Type == TEXT("box"))
			{
				// 상자: at = 모서리, size = [x, y, z], z = 띄울 층
				FIntVector BoxSize(2, 2, 2);
				if (!ReadInt2(Item, TEXT("at"), At))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": at 필요"));
					continue;
				}
				ReadInt3(Item, TEXT("size"), BoxSize);
				BoxSize.Z = BoxSize.Z > 0 ? BoxSize.Z : BoxSize.X;
				const int32 BaseZ = ReadInt(Item, TEXT("z"), 0);
				for (int32 Z = 0; Z < BoxSize.Z; ++Z)
				{
					for (int32 Dy = 0; Dy < BoxSize.Y; ++Dy)
					{
						for (int32 Dx = 0; Dx < BoxSize.X; ++Dx)
						{
							Build.Set(At.X + Dx, At.Y + Dy, BaseZ + Z, Block);
						}
					}
				}
			}
			else if (Type == TEXT("pillar"))
			{
				if (!ReadInt2(Item, TEXT("at"), At))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": at 필요"));
					continue;
				}
				const int32 Height = ReadInt(Item, TEXT("height"), 4);
				for (int32 Z = 0; Z < Height; ++Z)
				{
					Build.Set(At.X, At.Y, Z, Block);
				}
			}
			else if (Type == TEXT("fill"))
			{
				// 직육면체 채우기: from / to = [x, y, z] (z는 구조물 층, 음수면 지면 층)
				FIntVector A, B;
				if (!ReadInt3(Item, TEXT("from"), A) || !ReadInt3(Item, TEXT("to"), B))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": from / to 필요 ([x, y, z])"));
					continue;
				}
				for (int32 Z = FMath::Min(A.Z, B.Z); Z <= FMath::Max(A.Z, B.Z); ++Z)
				{
					for (int32 Y = FMath::Min(A.Y, B.Y); Y <= FMath::Max(A.Y, B.Y); ++Y)
					{
						for (int32 X = FMath::Min(A.X, B.X); X <= FMath::Max(A.X, B.X); ++X)
						{
							Build.Set(X, Y, Z, Block);
						}
					}
				}
			}
			else if (Type == TEXT("hole"))
			{
				// 구멍: 지면을 depth 층 파낸다 (지면 층 수 이상이면 허공)
				if (!ReadInt2(Item, TEXT("from"), From) || !ReadInt2(Item, TEXT("to"), To))
				{
					Spec.Warnings.Add(Describe(i, Type) + TEXT(": from / to 필요"));
					continue;
				}
				const int32 Depth = FMath::Max(1, ReadInt(Item, TEXT("depth"), Spec.GroundLayers));
				for (int32 Y = FMath::Min(From.Y, To.Y); Y <= FMath::Max(From.Y, To.Y); ++Y)
				{
					for (int32 X = FMath::Min(From.X, To.X); X <= FMath::Max(From.X, To.X); ++X)
					{
						for (int32 D = 0; D < Depth; ++D)
						{
							Build.SetGround(X, Y, D, ECRBlock::Empty);
						}
					}
				}
			}
			else
			{
				Spec.Warnings.Add(FString::Printf(TEXT("structures[%d]: 모르는 type '%s' (wall / tower / pyramid / bridge / box / pillar / fill / hole)"), i, *Type));
			}
		}
	}

	// 오브젝트 (훈련장 허수아비, 기믹)
	const TArray<TSharedPtr<FJsonValue>>* Objects = nullptr;
	if (Root->TryGetArrayField(TEXT("objects"), Objects))
	{
		for (int32 i = 0; i < Objects->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> Item = (*Objects)[i]->AsObject();
			FString Type;
			if (!Item.IsValid() || !Item->TryGetStringField(TEXT("type"), Type))
			{
				Spec.Warnings.Add(FString::Printf(TEXT("objects[%d]: type 필요"), i));
				continue;
			}
			Type = Type.ToLower();
			FCRMapObjectSpec Object;
			Object.Speed = ReadInt(Item, TEXT("speed"), 200);
			if (Type == TEXT("dummy_grid"))
			{
				// 허수아비 여러 마리: from ~ to 사각형 안에 spacing 칸 간격
				FIntPoint From, To;
				if (!ReadInt2(Item, TEXT("from"), From) || !ReadInt2(Item, TEXT("to"), To))
				{
					Spec.Warnings.Add(FString::Printf(TEXT("objects[%d] (dummy_grid): from / to 필요"), i));
					continue;
				}
				const int32 Spacing = FMath::Max(2, ReadInt(Item, TEXT("spacing"), 4));
				for (int32 Y = FMath::Min(From.Y, To.Y); Y <= FMath::Max(From.Y, To.Y); Y += Spacing)
				{
					for (int32 X = FMath::Min(From.X, To.X); X <= FMath::Max(From.X, To.X); X += Spacing)
					{
						FCRMapObjectSpec& Dummy = Spec.Objects.AddDefaulted_GetRef();
						Dummy.Type = ECRMapObject::Dummy;
						Dummy.At = FIntPoint(X, Y);
					}
				}
				continue;
			}
			if (Type == TEXT("dummy"))
			{
				Object.Type = ECRMapObject::Dummy;
			}
			else if (Type == TEXT("dps_dummy"))
			{
				Object.Type = ECRMapObject::DpsDummy;
			}
			else if (Type == TEXT("moving_target"))
			{
				Object.Type = ECRMapObject::MovingTarget;
				if (!ReadInt2(Item, TEXT("from"), Object.At) || !ReadInt2(Item, TEXT("to"), Object.To))
				{
					Spec.Warnings.Add(FString::Printf(TEXT("objects[%d] (moving_target): from / to 필요"), i));
					continue;
				}
				Spec.Objects.Add(Object);
				continue;
			}
			else if (Type == TEXT("barrel"))
			{
				Object.Type = ECRMapObject::Barrel;
			}
			else
			{
				Spec.Warnings.Add(FString::Printf(TEXT("objects[%d]: 모르는 type '%s' (dummy / dummy_grid / dps_dummy / moving_target / barrel)"), i, *Type));
				continue;
			}
			if (!ReadInt2(Item, TEXT("at"), Object.At))
			{
				Spec.Warnings.Add(FString::Printf(TEXT("objects[%d] (%s): at 필요"), i, *Type));
				continue;
			}
			Spec.Objects.Add(Object);
		}
	}

	OutSpec = MoveTemp(Spec);
	return true;
}
