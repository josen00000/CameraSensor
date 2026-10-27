// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SfcCaptureTypes.h"
#include "SfcPropSim.h"
#include "SfcPropSpawn.h"
#include "SfcSimClockSubsystem.h"
#include "sfc/capture_trigger.h"
#include "sfc/fixed_step_clock.h"
#include "sfc/wall_time_budget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SfcSceneSimTest
{
	inline const FVector kDefaultArenaHalfExtents(1000.0f, 1000.0f, 200.0f);
	inline constexpr int32 kDefaultPropCount = 200;
	inline constexpr int32 kSimTicks = 3600;

	struct WallSimRunResult
	{
		uint64 TickCount = 0;
		uint64 MicrosecondsFed = 0;
		uint64 HashAtTick600 = 0;
		uint64 HashAtTick3600 = 0;
		TArray<uint64> PerTickHashes;
		TArray<FSfcCaptureStamp> CaptureHistory;
	};

	inline void InitSimStatesForSeed(uint64 Seed, int32 PropCount, TArray<sfc::PropSimState>& OutSimStates)
	{
		TArray<FSfcPropState> PropStates;
		FSfcPropSpawn::BuildStates(Seed, PropCount, kDefaultArenaHalfExtents, PropStates);
		FSfcPropSim::BuildSimStatesFromSpawn(Seed, PropStates, OutSimStates);
	}

	inline WallSimRunResult RunSimWallSchedule(
		uint64 Seed,
		int32 PropCount,
		const std::vector<uint64_t>& FrameDeltasUs,
		bool bRecordCaptures)
	{
		WallSimRunResult Result;

		TArray<sfc::PropSimState> SimStates;
		InitSimStatesForSeed(Seed, PropCount, SimStates);
		const sfc::ArenaMilli Arena = FSfcPropSim::ArenaToSimMilli(kDefaultArenaHalfExtents);

		auto clock = sfc::FixedStepClock::Create(
			USfcSimClockSubsystem::kSimPeriodMicroseconds,
			USfcSimClockSubsystem::kSimTicksPerSecond);
		if (!clock.has_value())
		{
			return Result;
		}

		sfc::CaptureTrigger Trigger;
		Trigger.Reset(6);

		sfc::WallBudgetRunCallbacks Callbacks;
		Callbacks.on_tick = [&](uint64_t TickIndex)
		{
			FSfcPropSim::StepSimTick(SimStates, Arena);
			const uint64 Hash = FSfcPropSim::HashSimStates(SimStates);
			Result.PerTickHashes.Add(Hash);
			if (TickIndex == 600)
			{
				Result.HashAtTick600 = Hash;
			}
			if (TickIndex == static_cast<uint64_t>(kSimTicks))
			{
				Result.HashAtTick3600 = Hash;
			}
			if (bRecordCaptures)
			{
				const std::optional<sfc::CaptureStamp> Stamp = Trigger.OnSimTick(TickIndex);
				if (Stamp.has_value())
				{
					FSfcCaptureStamp Record;
					Record.Seq = static_cast<int64>(Stamp->seq);
					Record.TickIndex = static_cast<int64>(Stamp->tick_index);
					Result.CaptureHistory.Add(Record);
				}
			}
		};

		const sfc::WallBudgetRunResult BudgetResult =
			sfc::RunWallSchedule(*clock, FrameDeltasUs, Callbacks);
		Result.TickCount = BudgetResult.ticks;
		Result.MicrosecondsFed = BudgetResult.microseconds_fed;
		return Result;
	}

	inline WallSimRunResult RunSimSixtySecondWallBudget(uint64 Seed, int32 PropCount, bool bHitch)
	{
		const std::vector<uint64_t> Base = sfc::BuildAlternatingWallSchedule(sfc::kSixtySecondWallBudgetUs);
		const std::vector<uint64_t> Schedule =
			bHitch ? sfc::BuildHitchWallSchedule(Base) : Base;
		return RunSimWallSchedule(Seed, PropCount, Schedule, true);
	}

	inline WallSimRunResult RunSimFpsWallBudget(uint64 Seed, int32 PropCount, uint64 FramesPerSecond)
	{
		const std::vector<uint64_t> Schedule =
			sfc::BuildFpsWallSchedule(sfc::kSixtySecondWallBudgetUs, FramesPerSecond);
		return RunSimWallSchedule(Seed, PropCount, Schedule, false);
	}

	inline bool RunPerTickHashesDirect(uint64 Seed, int32 PropCount, TArray<uint64>& OutPerTickHashes)
	{
		OutPerTickHashes.Reset();

		TArray<sfc::PropSimState> SimStates;
		InitSimStatesForSeed(Seed, PropCount, SimStates);
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
		if (A.Num() != B.Num())
		{
			return FMath::Min(A.Num(), B.Num()) + 1;
		}
		return -1;
	}
}

#endif
