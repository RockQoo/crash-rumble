#include "Map/CRBlockArena.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/CRGameSettings.h"
#include "Core/CRTuningSettings.h"
#include "Core/CRTuningSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Feedback/CRFxSubsystem.h"
#include "Feedback/CRScreenFxSubsystem.h"
#include "Character/CRCharacter.h"
#include "Map/CRExplosiveBarrel.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCRBlockArena, Log, All);

namespace
{
	constexpr float BlockSize = 50.f;
	constexpr float HiddenDepth = 100000.f;       // 부서진 블럭 인스턴스를 숨기는 깊이 (콜리전도 같이 치운다)
	constexpr float RegenRetryDelay = 0.25f;     // 이웃이 없거나 캐릭터가 있으면 미룸
	constexpr float FileCheckInterval = 1.f;
	constexpr float FallVoidDepth = 2000.f;
	constexpr int32 MaxFallDebrisPerFrame = 10;  // 한 프레임에 잔해는 최대 10개, 먼지 링은 2개
	constexpr int32 MaxFallRingsPerFrame = 2;
	constexpr float CrystalSparkleInterval = 0.7f;
	const FIntVector Neighbors[] = { FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1) };

	float EaseOutBack(float T)
	{
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.f;
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Square(T - 1.f);
	}

	float CellNoise(int32 Cell)
	{
		return FMath::Frac(FMath::Sin(Cell * 12.9898f) * 43758.5453f);
	}

	FAutoConsoleCommandWithWorldAndArgs LoadCommand(
		TEXT("CRMapLoad"),
		TEXT("블럭 맵 불러오기: CRMapLoad <이름> (Content/MapData/<이름>.json)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FString Message;
			ACRBlockArena::LoadMap(World, Args.Num() > 0 ? Args[0] : TEXT("arena"), Message);
		}));

	// 시험용: 칸 좌표 (x, y, 구조물 z) 중심 반경 Radius(cm)를 지운다. Delay초 뒤 실행 (시작 명령에서 맵이 생긴 뒤 쓰려고)
	FAutoConsoleCommandWithWorldAndArgs BlastCommand(
		TEXT("CRMapBlast"),
		TEXT("블럭 폭파 시험: CRMapBlast <x> <y> <z> [반경 cm = 80] [지연 초 = 0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 3)
			{
				return;
			}
			const FIntVector Cell(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
			const float Radius = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 80.f;
			const float Delay = Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.f;
			TWeakObjectPtr<UWorld> WeakWorld = World;
			auto Blast = [WeakWorld, Cell, Radius]()
			{
				if (ACRBlockArena* Arena = ACRBlockArena::Find(WeakWorld.Get()))
				{
					Arena->DestroyCellSphere(Cell, Radius);
				}
			};
			if (Delay > 0.f)
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Blast), Delay, false);
			}
			else
			{
				Blast();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ClearCommand(
		TEXT("CRMapClear"),
		TEXT("블럭 맵 지우기 (원래 바닥으로)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ACRBlockArena::ClearMap(World);
		}));
}

ACRBlockArena::ACRBlockArena()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

ACRBlockArena* ACRBlockArena::Find(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ACRBlockArena> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

ACRBlockArena* ACRBlockArena::FromComponent(const UPrimitiveComponent* Component)
{
	return Component ? Cast<ACRBlockArena>(Component->GetOwner()) : nullptr;
}

ACRBlockArena* ACRBlockArena::LoadMap(UWorld* World, const FString& InMapName, FString& OutMessage)
{
	if (!World)
	{
		return nullptr;
	}
	ACRBlockArena* Arena = Find(World);
	if (!Arena)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Arena = World->SpawnActor<ACRBlockArena>(Params);
	}
	if (!Arena || !Arena->Load(InMapName, OutMessage))
	{
		UE_LOG(LogCRBlockArena, Warning, TEXT("%s"), *OutMessage);
		return nullptr;
	}
	return Arena;
}

void ACRBlockArena::ClearMap(UWorld* World)
{
	while (ACRBlockArena* Arena = Find(World))
	{
		Arena->Destroy();
	}
}

void ACRBlockArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearObjects();
	SetFloorsHidden(false);
	Super::EndPlay(EndPlayReason);
}

bool ACRBlockArena::Load(const FString& InMapName, FString& OutMessage)
{
	FCRMapSpec Spec;
	FString Error;
	if (!CRMapFile::LoadFile(InMapName, Spec, Error))
	{
		OutMessage = StatusMessage = Error;
		return false;
	}
	for (const FString& Warning : Spec.Warnings)
	{
		UE_LOG(LogCRBlockArena, Warning, TEXT("[%s] %s"), *InMapName, *Warning);
	}
	MapName = InMapName;
	MapPath = CRMapFile::ResolvePath(InMapName);
	MapTimestamp = IFileManager::Get().GetTimeStamp(*MapPath);
	const int32 WarningCount = Spec.Warnings.Num();
	const TArray<FCRMapObjectSpec> Objects = Spec.Objects;
	Rebuild(MoveTemp(Spec));
	SpawnObjects(Objects);
	OutMessage = StatusMessage = FString::Printf(TEXT("맵 '%s' 불러옴 (%s) · 블럭 %d · 오브젝트 %d%s"), *MapName, *FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		SolidCount, SpawnedObjects.Num(), WarningCount > 0 ? *FString::Printf(TEXT(" · 경고 %d (Output Log)"), WarningCount) : TEXT(""));
	UE_LOG(LogCRBlockArena, Log, TEXT("%s"), *StatusMessage);
	return true;
}

void ACRBlockArena::Rebuild(FCRMapSpec&& Spec)
{
	for (UInstancedStaticMeshComponent* Component : TypeComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	TypeComponents.Reset();
	if (FallingComponent)
	{
		FallingComponent->DestroyComponent();
		FallingComponent = nullptr;
	}
	Falling.Reset();
	FallingDrawn = 0;
	Regrowing.Reset();
	RegenQueue.Reset();
	RegenCursor = 0;

	SizeX = Spec.SizeX;
	SizeY = Spec.SizeY;
	SizeZ = Spec.GetSizeZ();
	GroundLayers = Spec.GroundLayers;
	// 지면 맨 위가 z = 0, 가운데가 원점
	Origin = FVector(-SizeX * BlockSize * 0.5f, -SizeY * BlockSize * 0.5f, -GroundLayers * BlockSize);

	Cells = Spec.Cells;
	Original = MoveTemp(Spec.Cells);
	Health.SetNumUninitialized(Cells.Num());
	Instance.Init(INDEX_NONE, Cells.Num());
	RegenAt.Init(0.f, Cells.Num());

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Toon = UCRGameSettings::LoadToonMaterial();
	auto MakeComponent = [this, Cube, Toon](bool bCollision)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
		Component->SetStaticMesh(Cube);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetGenerateOverlapEvents(false);
		Component->SetNumCustomDataFloats(CRMaterialData::InstanceFloats);
		if (bCollision)
		{
			// 캐릭터 이동, 발 위치, 블롭 그림자, 무기 판정의 바닥 트레이스가 그대로 동작한다
			Component->SetCollisionProfileName(TEXT("BlockAll"));
			Component->SetCollisionObjectType(ECC_WorldStatic);
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Component->SetupAttachment(Root);
		Component->RegisterComponent();
		if (Toon)
		{
			Component->SetMaterial(0, Toon);
		}
		Component->SetCustomPrimitiveDataFloat(CRMaterialData::NoRimIndex, 1.f);
		return Component;
	};

	const int32 TypeCount = static_cast<int32>(ECRBlock::Count);
	TypeComponents.SetNum(TypeCount);
	TArray<TArray<int32>> CellsByType;
	CellsByType.SetNum(TypeCount);
	SolidCount = 0;
	for (int32 Cell = 0; Cell < Cells.Num(); ++Cell)
	{
		const ECRBlock Type = static_cast<ECRBlock>(Cells[Cell]);
		Health[Cell] = static_cast<uint8>(CRMapFile::GetBlockHealth(Type));
		if (Type != ECRBlock::Empty)
		{
			CellsByType[static_cast<int32>(Type)].Add(Cell);
			++SolidCount;
		}
	}

	for (int32 Type = 1; Type < TypeCount; ++Type)
	{
		const TArray<int32>& TypeCells = CellsByType[Type];
		if (TypeCells.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Component = MakeComponent(true);
		TypeComponents[Type] = Component;
		TArray<FTransform> Transforms;
		Transforms.Reserve(TypeCells.Num());
		for (const int32 Cell : TypeCells)
		{
			Transforms.Add(GetCellTransform(Cell));
		}
		Component->AddInstances(Transforms, false, true);
		for (int32 i = 0; i < TypeCells.Num(); ++i)
		{
			Instance[TypeCells[i]] = i;
			const FVector4f Color = GetCellColor(TypeCells[i], static_cast<ECRBlock>(Type));
			const float Data[] = { Color.X, Color.Y, Color.Z, Color.W };
			Component->SetCustomData(i, Data, false);
		}
		Component->UpdateBounds();
		Component->MarkRenderStateDirty();
	}
	FallingComponent = MakeComponent(false);
	SetFloorsHidden(true);
}

float ACRBlockArena::GetColumnTopZ(int32 X, int32 Y) const
{
	X = FMath::Clamp(X, 0, SizeX - 1);
	Y = FMath::Clamp(Y, 0, SizeY - 1);
	for (int32 Z = SizeZ - 1; Z >= 0; --Z)
	{
		if (IsSolid(X + Y * SizeX + Z * SizeX * SizeY))
		{
			return Origin.Z + (Z + 1) * BlockSize;
		}
	}
	return Origin.Z + GroundLayers * BlockSize;
}

void ACRBlockArena::ClearObjects()
{
	for (AActor* Actor : SpawnedObjects)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedObjects.Reset();
}

void ACRBlockArena::SpawnObjects(const TArray<FCRMapObjectSpec>& Objects)
{
	ClearObjects();
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	auto CellTop = [this](const FIntPoint& Cell, float Height)
	{
		const FVector Center = Origin + FVector(Cell.X + 0.5f, Cell.Y + 0.5f, 0.f) * BlockSize;
		return FVector(Center.X, Center.Y, GetColumnTopZ(Cell.X, Cell.Y) + Height);
	};
	constexpr float CharacterHalfHeight = 101.f;
	constexpr float BarrelHalfHeight = 46.f;

	for (const FCRMapObjectSpec& Object : Objects)
	{
		if (Object.Type == ECRMapObject::Barrel)
		{
			if (ACRExplosiveBarrel* Barrel = World->SpawnActor<ACRExplosiveBarrel>(CellTop(Object.At, BarrelHalfHeight), FRotator::ZeroRotator, Params))
			{
				SpawnedObjects.Add(Barrel);
			}
			continue;
		}
		// 허수아비는 맵 가운데(플레이어)를 바라본다
		const FVector Location = CellTop(Object.At, CharacterHalfHeight);
		const FVector ToCenter = GetCenterTop() - Location;
		const FRotator Facing(0.f, ToCenter.SizeSquared2D() > 1.f ? ToCenter.Rotation().Yaw : 180.f, 0.f);
		ACRCharacter* Dummy = World->SpawnActor<ACRCharacter>(Location, Facing, Params);
		if (!Dummy)
		{
			continue;
		}
		switch (Object.Type)
		{
		case ECRMapObject::DpsDummy:
			Dummy->SetupTraining(ECRTrainingRole::DpsDummy);
			break;
		case ECRMapObject::MovingTarget:
			Dummy->SetupTraining(ECRTrainingRole::MovingTarget, Location, CellTop(Object.To, CharacterHalfHeight), Object.Speed);
			break;
		default:
			Dummy->SetupTraining(ECRTrainingRole::Dummy);
			break;
		}
		SpawnedObjects.Add(Dummy);
	}
}

void ACRBlockArena::SetFloorsHidden(bool bHideFloors)
{
	if (bHideFloors)
	{
		if (HiddenFloors.Num() > 0)
		{
			return;
		}
		// 템플릿 바닥(이름에 Floor)을 치워 맵 아래가 허공이 되게 한다
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			if (It->GetName().Contains(TEXT("Floor")) || It->GetActorNameOrLabel().Contains(TEXT("Floor")))
			{
				if (UStaticMeshComponent* Mesh = It->GetStaticMeshComponent())
				{
					Mesh->SetVisibility(false);
					Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					HiddenFloors.Add(Mesh);
				}
			}
		}
	}
	else
	{
		for (UStaticMeshComponent* Mesh : HiddenFloors)
		{
			if (IsValid(Mesh))
			{
				Mesh->SetVisibility(true);
				Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
		HiddenFloors.Reset();
	}
}

FIntVector ACRBlockArena::CellCoord(int32 Cell) const
{
	const int32 Layer = SizeX * SizeY;
	return FIntVector(Cell % SizeX, (Cell % Layer) / SizeX, Cell / Layer);
}

FVector ACRBlockArena::CellCenter(int32 Cell) const
{
	const FIntVector C = CellCoord(Cell);
	return Origin + (FVector(C.X, C.Y, C.Z) + FVector(0.5f)) * BlockSize;
}

int32 ACRBlockArena::CellAt(const FVector& WorldPoint) const
{
	const FVector Local = (WorldPoint - Origin) / BlockSize;
	const int32 X = FMath::FloorToInt(Local.X);
	const int32 Y = FMath::FloorToInt(Local.Y);
	const int32 Z = FMath::FloorToInt(Local.Z);
	if (X < 0 || Y < 0 || Z < 0 || X >= SizeX || Y >= SizeY || Z >= SizeZ)
	{
		return INDEX_NONE;
	}
	return X + Y * SizeX + Z * SizeX * SizeY;
}

FVector2D ACRBlockArena::GetHalfSize() const
{
	return FVector2D(SizeX, SizeY) * BlockSize * 0.5f;
}

FVector ACRBlockArena::GetCenterTop() const
{
	return FVector(Origin.X + SizeX * BlockSize * 0.5f, Origin.Y + SizeY * BlockSize * 0.5f, Origin.Z + GroundLayers * BlockSize);
}

bool ACRBlockArena::HasSolidNeighbor(int32 Cell) const
{
	const FIntVector C = CellCoord(Cell);
	for (const FIntVector& Offset : Neighbors)
	{
		const FIntVector N = C + Offset;
		if (N.X >= 0 && N.Y >= 0 && N.Z >= 0 && N.X < SizeX && N.Y < SizeY && N.Z < SizeZ
			&& IsSolid(N.X + N.Y * SizeX + N.Z * SizeX * SizeY))
		{
			return true;
		}
	}
	return false;
}

FVector4f ACRBlockArena::GetCellColor(int32 Cell, ECRBlock Type, float Brightness) const
{
	// 같은 색 블럭끼리 경계가 보이도록 체크무늬와 약간의 노이즈를 색에 구워 넣는다
	const FIntVector C = CellCoord(Cell);
	const float Checker = ((C.X + C.Y + C.Z) % 2 == 0) ? 1.f : 0.92f;
	const float Noise = 1.f + (CellNoise(Cell) - 0.5f) * 0.12f;
	const FLinearColor Base = CRMapFile::GetBlockColor(Type) * Checker * Noise * Brightness;
	return FVector4f(Base.R, Base.G, Base.B, CRMapFile::IsGlowBlock(Type) ? 1.f : 0.f);
}

FTransform ACRBlockArena::GetCellTransform(int32 Cell, float Scale) const
{
	return FTransform(FQuat::Identity, CellCenter(Cell), FVector(BlockSize / 100.f * Scale));
}

void ACRBlockArena::ShowCell(int32 Cell, float Scale, float Brightness)
{
	const ECRBlock Type = static_cast<ECRBlock>(Original[Cell]);
	UInstancedStaticMeshComponent* Component = TypeComponents.IsValidIndex(static_cast<int32>(Type)) ? TypeComponents[static_cast<int32>(Type)].Get() : nullptr;
	if (!Component || Instance[Cell] == INDEX_NONE)
	{
		return;
	}
	Component->UpdateInstanceTransform(Instance[Cell], GetCellTransform(Cell, Scale), true, false, true);
	const FVector4f Color = GetCellColor(Cell, Type, Brightness);
	const float Data[] = { Color.X, Color.Y, Color.Z, Color.W };
	Component->SetCustomData(Instance[Cell], Data, false);
	MarkDirty(Type);
}

void ACRBlockArena::HideCell(int32 Cell)
{
	const ECRBlock Type = static_cast<ECRBlock>(Original[Cell]);
	UInstancedStaticMeshComponent* Component = TypeComponents.IsValidIndex(static_cast<int32>(Type)) ? TypeComponents[static_cast<int32>(Type)].Get() : nullptr;
	if (!Component || Instance[Cell] == INDEX_NONE)
	{
		return;
	}
	FTransform Hidden = GetCellTransform(Cell);
	Hidden.AddToTranslation(FVector(0.f, 0.f, -HiddenDepth));
	Component->UpdateInstanceTransform(Instance[Cell], Hidden, true, false, true);
	MarkDirty(Type);
}

void ACRBlockArena::FlushDirty()
{
	if (DirtyTypes == 0)
	{
		return;
	}
	// UE 5.8 인스턴스 메시는 바뀐 인스턴스만 알아서 GPU로 보낸다.
	// MarkRenderStateDirty는 종류 전체(수만 개)를 다시 만들므로 옛 방식(비교용)에서만 부른다
	if (!UCRTuningSubsystem::GetSettings(this).bBenchLegacyBlocks)
	{
		DirtyTypes = 0;
		return;
	}
	for (int32 Type = 1; Type < TypeComponents.Num(); ++Type)
	{
		if ((DirtyTypes & (1 << Type)) && TypeComponents[Type])
		{
			TypeComponents[Type]->MarkRenderStateDirty();
		}
	}
	DirtyTypes = 0;
}

void ACRBlockArena::ScheduleRegen(int32 Cell)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (!Tuning.bBlockRegen)
	{
		return;
	}
	// 층마다 조금씩 늦게: 크레이터가 바닥부터 차오르고 탑이 아래부터 다시 쌓인다
	const float Delay = Tuning.BlockRegenDelay + FMath::FRandRange(0.f, Tuning.BlockRegenRandom) + CellCoord(Cell).Z * Tuning.BlockRegenPerLayer;
	if (RegenAt[Cell] <= 0.f)
	{
		RegenQueue.Add(Cell);
	}
	RegenAt[Cell] = GetWorld()->GetTimeSeconds() + Delay;
}

void ACRBlockArena::RemoveCell(int32 Cell, bool bDebris, const FVector& Normal)
{
	const ECRBlock Type = static_cast<ECRBlock>(Cells[Cell]);
	Cells[Cell] = 0;
	--SolidCount;
	HideCell(Cell);
	ScheduleRegen(Cell);
	if (bDebris && UCRTuningSubsystem::GetSettings(this).bCubeDebris)
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
		{
			// 부서진 블럭과 같은 색 큐브 잔해
			const FLinearColor Color = CRMapFile::GetBlockColor(Type);
			const FVector Center = CellCenter(Cell);
			Fx->SpawnDebris(Center, Normal, 6, Color, Color * 0.7f, 1.3f, Center.Z - BlockSize * 0.5f);
		}
	}
}

int32 ACRBlockArena::DamageAt(const FVector& Point, const FVector& Normal, int32 Damage)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_BlockDamageAt);
	if (Damage <= 0)
	{
		return 0;
	}
	// 맞은 면의 법선 반대쪽으로 5cm 들어간 점 → 이웃이 아니라 맞은 블럭이 잡힌다
	const FVector N = Normal.GetSafeNormal();
	const int32 Cell = CellAt(Point - N * 5.f);
	if (!IsSolid(Cell))
	{
		return 0;
	}
	const int32 Left = int32(Health[Cell]) - Damage;
	if (Left > 0)
	{
		Health[Cell] = static_cast<uint8>(Left);
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this); Fx && UCRTuningSubsystem::GetSettings(this).bCubeDebris)
		{
			const FLinearColor Color = CRMapFile::GetBlockColor(static_cast<ECRBlock>(Cells[Cell]));
			Fx->SpawnDebris(Point, N, 2, Color, Color * 0.7f, 0.8f, CellCenter(Cell).Z - BlockSize * 0.5f);
		}
		return 0;
	}
	RemoveCell(Cell, true, N);
	RequestCollapse();
	return 1;
}

int32 ACRBlockArena::DestroySphere(const FVector& Center, float Radius)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_BlockDestroySphere);
	if (Radius <= 0.f || Cells.Num() == 0)
	{
		return 0;
	}
	const FVector Min = (Center - FVector(Radius) - Origin) / BlockSize;
	const FVector Max = (Center + FVector(Radius) - Origin) / BlockSize;
	int32 Count = 0;
	ECRBlock FirstType = ECRBlock::Empty;
	for (int32 Z = FMath::Max(0, FMath::FloorToInt(Min.Z)); Z <= FMath::Min(SizeZ - 1, FMath::FloorToInt(Max.Z)); ++Z)
	{
		for (int32 Y = FMath::Max(0, FMath::FloorToInt(Min.Y)); Y <= FMath::Min(SizeY - 1, FMath::FloorToInt(Max.Y)); ++Y)
		{
			for (int32 X = FMath::Max(0, FMath::FloorToInt(Min.X)); X <= FMath::Min(SizeX - 1, FMath::FloorToInt(Max.X)); ++X)
			{
				const int32 Cell = X + Y * SizeX + Z * SizeX * SizeY;
				if (IsSolid(Cell) && FVector::DistSquared(CellCenter(Cell), Center) <= FMath::Square(Radius))
				{
					if (FirstType == ECRBlock::Empty)
					{
						FirstType = static_cast<ECRBlock>(Cells[Cell]);
					}
					RemoveCell(Cell, false, FVector::UpVector);
					++Count;
				}
			}
		}
	}
	// 블럭마다 잔해를 만들면 수백 개가 되므로 크레이터 하나당 잔해 한 무더기
	if (Count > 0)
	{
		if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this); Fx && UCRTuningSubsystem::GetSettings(this).bCubeDebris)
		{
			const FLinearColor Color = CRMapFile::GetBlockColor(FirstType);
			Fx->SpawnDebris(Center, FVector::UpVector, 16, Color, Color * 0.65f, 1.5f, Center.Z - Radius);
		}
		RequestCollapse();
	}
	return Count;
}

int32 ACRBlockArena::DestroyCellSphere(const FIntVector& StructureCell, float Radius)
{
	const FVector Center = Origin + (FVector(StructureCell.X, StructureCell.Y, StructureCell.Z + GroundLayers) + FVector(0.5f)) * BlockSize;
	return DestroySphere(Center, Radius);
}

void ACRBlockArena::RequestCollapse()
{
	// 옛 방식(비교용): 부서질 때마다 바로 검사하고 렌더 갱신
	if (UCRTuningSubsystem::GetSettings(this).bBenchLegacyBlocks)
	{
		CollapseUnsupported();
		FlushDirty();
		return;
	}
	bCollapsePending = true;
}

int32 ACRBlockArena::CollapseUnsupported()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_BlockCollapse);
	if (SizeZ <= GroundLayers)
	{
		return 0;
	}
	// 땅에 닿은 구조물 블럭에서 출발해 6방향 이웃으로 퍼져 나간다. 도달하지 못한 블럭 = 떨어짐
	// ("바로 아래만 보기"로 하면 다리 상판, 성문 위가 다 떨어진다)
	const int32 Layer = SizeX * SizeY;
	TBitArray<> Reached(false, Cells.Num());
	TArray<int32> Stack;
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const int32 Cell = X + Y * SizeX + GroundLayers * Layer;
			if (IsSolid(Cell) && IsSolid(Cell - Layer))
			{
				Reached[Cell] = true;
				Stack.Add(Cell);
			}
		}
	}
	while (Stack.Num() > 0)
	{
		const int32 Cell = Stack.Pop(EAllowShrinking::No);
		const FIntVector C = CellCoord(Cell);
		for (const FIntVector& Offset : Neighbors)
		{
			const FIntVector N = C + Offset;
			if (N.X < 0 || N.Y < 0 || N.Z < GroundLayers || N.X >= SizeX || N.Y >= SizeY || N.Z >= SizeZ)
			{
				continue;
			}
			const int32 Next = N.X + N.Y * SizeX + N.Z * Layer;
			if (IsSolid(Next) && !Reached[Next])
			{
				Reached[Next] = true;
				Stack.Add(Next);
			}
		}
	}

	int32 Fallen = 0;
	for (int32 Cell = GroundLayers * Layer; Cell < Cells.Num(); ++Cell)
	{
		if (IsSolid(Cell) && !Reached[Cell])
		{
			const ECRBlock Type = static_cast<ECRBlock>(Cells[Cell]);
			Cells[Cell] = 0;
			--SolidCount;
			HideCell(Cell);
			ScheduleRegen(Cell);
			// 처음에 옆으로 살짝 밀고 천천히 회전 → 덩어리째 떨어지며 조금씩 흐트러진다
			FFallingBlock& Block = Falling.AddDefaulted_GetRef();
			Block.Location = CellCenter(Cell);
			Block.Velocity = FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f);
			Block.SpinAxis = FMath::VRand();
			Block.SpinSpeed = FMath::FRandRange(0.5f, 1.5f);
			Block.Type = Type;
			++Fallen;
		}
	}

	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	if (Fallen >= Tuning.BlockCollapseImpact)
	{
		if (UCRScreenFxSubsystem* Screen = UCRScreenFxSubsystem::Get(this))
		{
			Screen->RequestImpactFrame();
			Screen->AddFocusLines(0.6f, 0.4f);
		}
	}
	return Fallen;
}

void ACRBlockArena::UpdateFalling(float DeltaSeconds)
{
	if (Falling.Num() == 0)
	{
		return;
	}
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this);
	int32 DebrisBudget = MaxFallDebrisPerFrame;
	int32 RingBudget = MaxFallRingsPerFrame;
	const float VoidZ = Origin.Z - FallVoidDepth;
	for (int32 i = Falling.Num() - 1; i >= 0; --i)
	{
		FFallingBlock& Block = Falling[i];
		Block.Velocity.Z -= Tuning.BlockFallGravity * DeltaSeconds;
		Block.Location += Block.Velocity * DeltaSeconds;
		Block.Rotation = FQuat(Block.SpinAxis, Block.SpinSpeed * DeltaSeconds) * Block.Rotation;

		// 착지 판정은 콜리전이 아니라 격자 조회 (밑면이 들어간 칸이 차 있나)
		const FVector Bottom = Block.Location - FVector(0.f, 0.f, BlockSize * 0.5f);
		if (Bottom.Z < VoidZ)
		{
			Falling.RemoveAtSwap(i);
			continue;
		}
		const int32 Below = CellAt(Bottom);
		if (IsSolid(Below))
		{
			if (Fx && Tuning.bCubeDebris && DebrisBudget-- > 0)
			{
				const FLinearColor Color = CRMapFile::GetBlockColor(Block.Type);
				Fx->SpawnDebris(Block.Location, FVector::UpVector, 6, Color, Color * 0.7f, 1.3f, Bottom.Z);
			}
			if (Fx && Tuning.bDustRing && RingBudget-- > 0)
			{
				Fx->SpawnDustRing(Bottom, 0.7f);
			}
			Falling.RemoveAtSwap(i);
		}
	}
}

void ACRBlockArena::FlushFalling()
{
	if (!FallingComponent || (Falling.Num() == 0 && FallingDrawn == 0))
	{
		return;
	}
	const int32 Need = Falling.Num();
	const int32 Have = FallingComponent->GetInstanceCount();
	const FTransform Hidden(FQuat::Identity, FVector(0.f, 0.f, -HiddenDepth), FVector::ZeroVector);
	if (Need > Have)
	{
		TArray<FTransform> Extra;
		Extra.Init(Hidden, FMath::Max(Need - Have, 32));
		FallingComponent->AddInstances(Extra, false, true);
	}
	for (int32 i = 0; i < Need; ++i)
	{
		const FFallingBlock& Block = Falling[i];
		FallingComponent->UpdateInstanceTransform(i, FTransform(Block.Rotation, Block.Location, FVector(BlockSize / 100.f)), true, false, true);
		const FLinearColor Color = CRMapFile::GetBlockColor(Block.Type);
		const float Data[] = { Color.R, Color.G, Color.B, CRMapFile::IsGlowBlock(Block.Type) ? 1.f : 0.f };
		FallingComponent->SetCustomData(i, Data, false);
	}
	for (int32 i = Need; i < FallingDrawn; ++i)
	{
		FallingComponent->UpdateInstanceTransform(i, Hidden, true, false, true);
	}
	FallingDrawn = Need;
	FallingComponent->UpdateBounds();
	FallingComponent->MarkRenderStateDirty();
}

void ACRBlockArena::UpdateRegen(float DeltaSeconds)
{
	const UCRTuningSettings& Tuning = UCRTuningSubsystem::GetSettings(this);
	const float Now = GetWorld()->GetTimeSeconds();
	int32 Budget = FMath::Max(Tuning.BlockRegenPerFrame, 1);
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(BlockSize * 0.5f - 2.f));
	FCollisionObjectQueryParams Pawns;
	Pawns.AddObjectTypesToQuery(ECC_Pawn);

	// 대기 칸이 수천 개여도 프레임당 검사(시간 비교, 겹침 검사)는 예산만큼만: 커서로 큐를 이어서 돈다.
	// 지운 자리는 맨 뒤 칸이 채우는데, 그 칸은 이번 바퀴에 이미 봤거나 다음 바퀴에 본다
	int32 Visits = FMath::Min(FMath::Max(Tuning.BlockRegenVisitPerFrame, 1), RegenQueue.Num());
	while (Visits-- > 0 && Budget > 0 && RegenQueue.Num() > 0)
	{
		if (RegenCursor <= 0 || RegenCursor > RegenQueue.Num())
		{
			RegenCursor = RegenQueue.Num();
		}
		const int32 i = --RegenCursor;
		const int32 Cell = RegenQueue[i];
		if (RegenAt[Cell] <= 0.f || IsSolid(Cell))
		{
			RegenAt[Cell] = 0.f;
			RegenQueue.RemoveAtSwap(i);
			continue;
		}
		if (Now < RegenAt[Cell])
		{
			continue;
		}
		// 공중에 혼자 생기지 않게 이웃이 있어야 하고 (맨 아래층 제외), 캐릭터가 그 칸에 있으면 미룬다 (끼임 방지)
		if ((CellCoord(Cell).Z > 0 && !HasSolidNeighbor(Cell))
			|| GetWorld()->OverlapAnyTestByObjectType(CellCenter(Cell), FQuat::Identity, Pawns, Box))
		{
			RegenAt[Cell] = Now + RegenRetryDelay;
			continue;
		}
		Cells[Cell] = Original[Cell];
		Health[Cell] = static_cast<uint8>(CRMapFile::GetBlockHealth(static_cast<ECRBlock>(Original[Cell])));
		RegenAt[Cell] = 0.f;
		++SolidCount;
		RegenQueue.RemoveAtSwap(i);
		ShowCell(Cell, Tuning.BlockRegrowStartScale, Tuning.BlockRegrowBrightness);
		Regrowing.Add({ Cell, 0.f });
		--Budget;
	}

	// 다시 자라는 연출: 5%에서 0.3초 동안 살짝 넘치게 커짐 + 3배 밝기에서 원래 색. 12fps 계단으로
	const bool bDraw = UCRFxSubsystem::IsFxDrawFrame(this);
	for (int32 i = Regrowing.Num() - 1; i >= 0; --i)
	{
		FRegrow& Grow = Regrowing[i];
		Grow.Age += DeltaSeconds;
		const float T = Grow.Age / FMath::Max(Tuning.BlockRegrowDuration, 0.001f);
		if (!IsSolid(Grow.Cell))
		{
			Regrowing.RemoveAtSwap(i);
			continue;
		}
		if (T >= 1.f)
		{
			ShowCell(Grow.Cell, 1.f, 1.f);
			Regrowing.RemoveAtSwap(i);
		}
		else if (bDraw)
		{
			const float Scale = FMath::Lerp(Tuning.BlockRegrowStartScale, 1.f, EaseOutBack(T));
			ShowCell(Grow.Cell, Scale, FMath::Lerp(Tuning.BlockRegrowBrightness, 1.f, T));
		}
	}
}

void ACRBlockArena::UpdateCrystalSparkle(float DeltaSeconds)
{
	SparkleTimer += DeltaSeconds;
	if (SparkleTimer < CrystalSparkleInterval || !UCRTuningSubsystem::GetSettings(this).bSparkle)
	{
		return;
	}
	SparkleTimer = 0.f;
	const UInstancedStaticMeshComponent* Crystals = TypeComponents.IsValidIndex(static_cast<int32>(ECRBlock::Crystal))
		? TypeComponents[static_cast<int32>(ECRBlock::Crystal)].Get() : nullptr;
	if (!Crystals || Crystals->GetInstanceCount() == 0)
	{
		return;
	}
	// 크리스탈 블럭 하나를 골라 반짝 (몇 번 시도해서 남아 있는 것)
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const int32 Cell = FMath::RandRange(0, Cells.Num() - 1);
		if (Original[Cell] == static_cast<uint8>(ECRBlock::Crystal) && IsSolid(Cell))
		{
			if (UCRFxSubsystem* Fx = UCRFxSubsystem::Get(this))
			{
				Fx->SpawnSparkle(CellCenter(Cell) + FMath::VRand() * BlockSize * 0.4f, 20.f);
			}
			return;
		}
	}
}

void ACRBlockArena::CheckFileChanged(float DeltaSeconds)
{
	FileCheckTimer += DeltaSeconds;
	if (FileCheckTimer < FileCheckInterval || MapPath.IsEmpty())
	{
		return;
	}
	FileCheckTimer = 0.f;
	// 맵 파일 핫 리로드: AI(또는 사람)가 파일을 고치면 바로 다시 만든다
	const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*MapPath);
	if (Stamp != MapTimestamp && Stamp != FDateTime::MinValue())
	{
		MapTimestamp = Stamp;
		FString Message;
		if (!Load(MapName, Message))
		{
			StatusMessage = TEXT("다시 불러오기 실패: ") + Message;
		}
	}
}

void ACRBlockArena::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CR_ArenaTick);
	Super::Tick(DeltaSeconds);
	if (bCollapsePending)
	{
		bCollapsePending = false;
		CollapseUnsupported();
	}
	CheckFileChanged(DeltaSeconds);
	UpdateFalling(DeltaSeconds);
	UpdateRegen(DeltaSeconds);
	UpdateCrystalSparkle(DeltaSeconds);
	FlushFalling();
	FlushDirty();
}
