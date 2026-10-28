#include "Core/SBActorPool.h"

#include "Engine/World.h"

USBActorPoolSubsystem* USBActorPoolSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<USBActorPoolSubsystem>() : nullptr;
}

AActor* USBActorPoolSubsystem::Acquire(UClass* Class, const FTransform& Transform)
{
	if (!Class)
	{
		return nullptr;
	}

	AActor* Actor = nullptr;
	FSBActorPoolList& List = FreeActors.FindOrAdd(Class);
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
	Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
	if (ISBPoolable* Poolable = Cast<ISBPoolable>(Actor))
	{
		Poolable->OnPoolAcquired();
	}
	return Actor;
}

void USBActorPoolSubsystem::Release(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	if (ISBPoolable* Poolable = Cast<ISBPoolable>(Actor))
	{
		Poolable->OnPoolReleased();
	}
	int32& Active = ActiveCounts.FindOrAdd(Actor->GetClass());
	Active = FMath::Max(0, Active - 1);
	FreeActors.FindOrAdd(Actor->GetClass()).Actors.Add(Actor);
}

void USBActorPoolSubsystem::Prewarm(UClass* Class, int32 Count)
{
	FSBActorPoolList& List = FreeActors.FindOrAdd(Class);
	while (List.Actors.Num() < Count)
	{
		AActor* Actor = SpawnPooled(Class);
		if (!Actor)
		{
			break;
		}
		if (ISBPoolable* Poolable = Cast<ISBPoolable>(Actor))
		{
			Poolable->OnPoolReleased();
		}
		List.Actors.Add(Actor);
	}
}

AActor* USBActorPoolSubsystem::SpawnPooled(UClass* Class)
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

int32 USBActorPoolSubsystem::GetFreeCount(UClass* Class) const
{
	const FSBActorPoolList* List = FreeActors.Find(Class);
	return List ? List->Actors.Num() : 0;
}
