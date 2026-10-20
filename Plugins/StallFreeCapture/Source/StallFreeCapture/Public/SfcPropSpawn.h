// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SfcPropTypes.h"

/** Deterministic prop layout from sfc::Pcg32 only (no engine RNG). */
class STALLFREECAPTURE_API FSfcPropSpawn
{
public:
	static constexpr int32 MaxPropCount = 65535;

	/** Clamps to [0, MaxPropCount]; sets OutWasClamped when RequestedCount > MaxPropCount. */
	static int32 SanitizePropCount(int32 RequestedCount, bool& OutWasClamped);

	static void BuildStates(uint64 Seed, int32 PropCount, const FVector& ArenaHalfExtents, TArray<FSfcPropState>& OutStates);

	static uint64 HashInitialState(const TArray<FSfcPropState>& States);
};
