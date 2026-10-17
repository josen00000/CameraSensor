// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SfcPropTypes.h"
#include "SeededPropSpawner.generated.h"

UCLASS()
class STALLFREECAPTURE_API ASeededPropSpawner : public AActor
{
	GENERATED_BODY()

public:
	ASeededPropSpawner();

	UPROPERTY(EditAnywhere, Category = "SFC|Spawn")
	uint64 Seed = 7;

	UPROPERTY(EditAnywhere, Category = "SFC|Spawn", meta = (ClampMin = "0"))
	int32 PropCount = 200;

	UPROPERTY(EditAnywhere, Category = "SFC|Spawn")
	FVector ArenaHalfExtents = FVector(1000.0f, 1000.0f, 200.0f);

	UPROPERTY(VisibleAnywhere, Category = "SFC|Spawn")
	TArray<FSfcPropState> PropStates;

	UPROPERTY(VisibleAnywhere, Category = "SFC|Spawn")
	uint64 InitialStateHash = 0;

	virtual void BeginPlay() override;

private:
	void SpawnPropsInWorld();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedPropActors;
};
