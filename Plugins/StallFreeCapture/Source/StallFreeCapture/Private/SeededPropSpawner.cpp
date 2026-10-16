// Copyright Epic Games, Inc. All Rights Reserved.

#include "SeededPropSpawner.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "SfcPropSpawn.h"
#include "StallFreeCapture.h"

ASeededPropSpawner::ASeededPropSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASeededPropSpawner::BeginPlay()
{
	Super::BeginPlay();

	bool bClamped = false;
	const int32 SafeCount = FSfcPropSpawn::SanitizePropCount(PropCount, bClamped);
	if (bClamped)
	{
		UE_LOG(
			LogStallFreeCapture,
			Error,
			TEXT("PropCount %d exceeds maximum %d; clamping to %d (IDs 1..%d, no wrap)."),
			PropCount,
			FSfcPropSpawn::MaxPropCount,
			SafeCount,
			SafeCount);
		PropCount = SafeCount;
	}

	FSfcPropSpawn::BuildStates(Seed, PropCount, ArenaHalfExtents, PropStates);
	InitialStateHash = FSfcPropSpawn::HashInitialState(PropStates);

	UE_LOG(
		LogStallFreeCapture,
		Log,
		TEXT("SeededPropSpawner seed=%llu props=%d initial_state_hash=%llu"),
		Seed,
		PropStates.Num(),
		InitialStateHash);

	SpawnPropsInWorld();
}

void ASeededPropSpawner::SpawnPropsInWorld()
{
	for (AActor* Actor : SpawnedPropActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedPropActors.Reset();

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!CubeMesh || !SphereMesh || !CylinderMesh)
	{
		UE_LOG(LogStallFreeCapture, Error, TEXT("Failed to load basic shape meshes for prop spawn."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FSfcPropState& State : PropStates)
	{
		UStaticMesh* Mesh = CubeMesh;
		switch (State.PropClass)
		{
		case ESfcPropClass::Sphere:
			Mesh = SphereMesh;
			break;
		case ESfcPropClass::Cylinder:
			Mesh = CylinderMesh;
			break;
		default:
			break;
		}

		const FTransform SpawnTransform(State.Rotation, State.Position, State.Scale);
		AStaticMeshActor* PropActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnTransform, SpawnParams);
		if (!PropActor)
		{
			continue;
		}

		UStaticMeshComponent* MeshComponent = PropActor->GetStaticMeshComponent();
		if (MeshComponent)
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetSimulatePhysics(false);
		}
		PropActor->SetActorEnableCollision(false);

		SpawnedPropActors.Add(PropActor);
	}
}
