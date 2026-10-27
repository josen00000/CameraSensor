// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SfcPropTypes.h"
#include "sfc/prop_sim.h"

/** UE bridge for integer sim state used by the seeded spawner and automation tests. */
class STALLFREECAPTURE_API FSfcPropSim
{
public:
	static sfc::ArenaMilli ArenaToSimMilli(const FVector& ArenaHalfExtents);

	static void BuildSimStatesFromSpawn(
		uint64 Seed,
		const TArray<FSfcPropState>& PropStates,
		TArray<sfc::PropSimState>& OutSimStates);

	static void SyncVisualStateFromSim(const sfc::PropSimState& SimState, FSfcPropState& PropState);

	static uint64 HashSimStates(const TArray<sfc::PropSimState>& SimStates);

	static void StepSimTick(
		TArray<sfc::PropSimState>& SimStates,
		const sfc::ArenaMilli& Arena,
		TArray<int32>* OutStepClampedPropIds = nullptr);
};
