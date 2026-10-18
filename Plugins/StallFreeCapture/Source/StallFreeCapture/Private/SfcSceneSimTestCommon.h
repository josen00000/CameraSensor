// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SfcPropSim.h"
#include "SfcPropSpawn.h"
#include "SfcSimClockSubsystem.h"
#include "sfc/fixed_step_clock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SfcSceneSimTest
{
	inline const FVector kDefaultArenaHalfExtents(1000.0f, 1000.0f, 200.0f);
	inline constexpr int32 kDefaultPropCount = 200;
	inline constexpr int32 kSimTicks = 3600;

	inline bool RunPerTickHashes(
		uint64 Seed,
		int32 PropCount,
		TArray<uint64>& OutPerTickHashes,
		bool bHitchAtTick1000)
	{
		OutPerTickHashes.Reset();

		TArray<FSfcPropState> PropStates;
		FSfcPropSpawn::BuildStates(Seed, PropCount, kDefaultArenaHalfExtents, PropStates);

		TArray<sfc::PropSimState> SimStates;
		FSfcPropSim::BuildSimStatesFromSpawn(Seed, PropStates, SimStates);
		const sfc::ArenaMilli Arena = FSfcPropSim::ArenaToSimMilli(kDefaultArenaHalfExtents);

		auto clock = sfc::FixedStepClock::Create(
			USfcSimClockSubsystem::kSimPeriodMicroseconds,
			USfcSimClockSubsystem::kSimTicksPerSecond);
		if (!clock.has_value())
		{
			return false;
		}

		int32 ticks_completed = 0;
		while (ticks_completed < kSimTicks)
		{
			uint64 DeltaMicroseconds = 16'667;
			if (bHitchAtTick1000 && (ticks_completed + 1) == 1000)
			{
				DeltaMicroseconds = 200'000;
			}

			const sfc::FixedStepClock::AdvanceResult Advance = clock->Advance(DeltaMicroseconds);
			for (uint64 step = 0; step < Advance.ticks_ready && ticks_completed < kSimTicks; ++step)
			{
				FSfcPropSim::StepSimTick(SimStates, Arena);
				++ticks_completed;
				OutPerTickHashes.Add(FSfcPropSim::HashSimStates(SimStates));
			}
		}

		return OutPerTickHashes.Num() == kSimTicks;
	}

	inline bool RunPerTickHashesDirect(uint64 Seed, int32 PropCount, TArray<uint64>& OutPerTickHashes)
	{
		OutPerTickHashes.Reset();

		TArray<FSfcPropState> PropStates;
		FSfcPropSpawn::BuildStates(Seed, PropCount, kDefaultArenaHalfExtents, PropStates);

		TArray<sfc::PropSimState> SimStates;
		FSfcPropSim::BuildSimStatesFromSpawn(Seed, PropStates, SimStates);
		const sfc::ArenaMilli Arena = FSfcPropSim::ArenaToSimMilli(kDefaultArenaHalfExtents);

		for (int32 tick = 0; tick < kSimTicks; ++tick)
		{
			FSfcPropSim::StepSimTick(SimStates, Arena);
			OutPerTickHashes.Add(FSfcPropSim::HashSimStates(SimStates));
		}

		return OutPerTickHashes.Num() == kSimTicks;
	}

	inline int32 FindFirstMismatchingTick(const TArray<uint64>& A, const TArray<uint64>& B)
	{
		const int32 Count = FMath::Min(A.Num(), B.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (A[Index] != B[Index])
			{
				return Index + 1;
			}
		}
		return -1;
	}
}

#endif
