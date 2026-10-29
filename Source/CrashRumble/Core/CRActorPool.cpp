#include "Core/CRActorPool.h"

#include "Engine/World.h"

UCRActorPoolSubsystem* UCRActorPoolSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCRActorPoolSubsystem>() : nullptr;
}

AActor* UCRActorPoolSubsystem::Acquire(UClass* Class, const FTransform& Transform)
{
	if (!Class)
	{
		return nullptr;
	}

	AActor* Actor = nullptr;
	FCRActorPoolList& List = FreeActors.FindOrAdd(Class);
	while (!Actor && List.Actors.Num() > 0)
	{
		Actor = List.Actors.Pop(EAllowShrinking::No);
		if (!IsValid(Actor))
		{
			Actor = nullptr;
		}
	}
	if (!Actor)
	{
		Actor = SpawnPooled(Class);
	}
	if (!Actor)
	{
		return nullptr;
	}

	++ActiveCounts.FindOrAdd(Class);
	LentActors.Add(Actor);
	Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (ICRPoolable* Poolable = Cast<ICRPoolable>(Actor))
	{
		Poolable->OnPoolAcquired();
	}
	return Actor;
}

void UCRActorPoolSubsystem::Release(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	if (LentActors.Remove(Actor) == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CRActorPool: 빌려주지 않은 액터 반납 무시 (중복 반납?) %s"), *Actor->GetName());
		return;
	}
	if (ICRPoolable* Poolable = Cast<ICRPoolable>(Actor))
	{
		Poolable->OnPoolReleased();
	}
	int32& Active = ActiveCounts.FindOrAdd(Actor->GetClass());
	Active = FMath::Max(0, Active - 1);
	FreeActors.FindOrAdd(Actor->GetClass()).Actors.Add(Actor);
}

void UCRActorPoolSubsystem::Prewarm(UClass* Class, int32 Count)
{
	FCRActorPoolList& List = FreeActors.FindOrAdd(Class);
	while (List.Actors.Num() < Count)
	{
		AActor* Actor = SpawnPooled(Class);
		if (!Actor)
		{
			break;
		}
		if (ICRPoolable* Poolable = Cast<ICRPoolable>(Actor))
		{
			Poolable->OnPoolReleased();
		}
		List.Actors.Add(Actor);
	}
}

AActor* UCRActorPoolSubsystem::SpawnPooled(UClass* Class)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(Class, FTransform::Identity, Params);
	if (Actor)
	{
		++TotalSpawned;
	}
	return Actor;
}

int32 UCRActorPoolSubsystem::GetFreeCount(UClass* Class) const
{
	const FCRActorPoolList* List = FreeActors.Find(Class);
	return List ? List->Actors.Num() : 0;
}
