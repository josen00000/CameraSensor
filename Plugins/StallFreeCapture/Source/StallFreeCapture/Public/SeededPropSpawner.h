// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SfcPropTypes.h"
#include "sfc/prop_sim.h"
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** One 60 Hz analytic sim tick with arena wall bounce (sim clock only). */
	void StepSim();

	uint64 HashState() const;

	uint64 GetSimTickIndex() const { return SimTickIndex; }

	const TArray<sfc::PropSimState>& GetSimStates() const { return SimStates; }

	void InitializeSimulationFromSpawn();

private:
	void SpawnPropsInWorld();
	void SyncActorsFromSim();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedPropActors;

	TArray<sfc::PropSimState> SimStates;
	sfc::ArenaMilli ArenaMilli;
	uint64 SimTickIndex = 0;
};
