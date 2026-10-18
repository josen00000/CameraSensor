// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcSimClockSubsystem.h"

#include "SeededPropSpawner.h"
#include "StallFreeCapture.h"

void USfcSimClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetSimulationClock();
}

void USfcSimClockSubsystem::Deinitialize()
{
	RegisteredSpawners.Reset();
	Super::Deinitialize();
}

void USfcSimClockSubsystem::ResetSimulationClock()
{
	SimClock = sfc::FixedStepClock::Create(kSimPeriodMicroseconds, kSimTicksPerSecond);
	SimTickIndex = 0;
	TickRemainderMicroseconds = 0.0;
	CaptureHistory.Reset();
	ConfigureCaptureTrigger();
}

void USfcSimClockSubsystem::ConfigureCaptureTrigger()
{
	bool bClamped = false;
	const uint32 SafeEveryN = sfc::SanitizeCaptureEveryNTicks(CaptureEveryNTicks, bClamped);
	if (bClamped)
	{
		UE_LOG(
			LogStallFreeCapture,
			Warning,
			TEXT("CaptureEveryNTicks was 0; clamping to 1 (no zero-interval capture)."));
		CaptureEveryNTicks = SafeEveryN;
	}
	CaptureTrigger.Reset(CaptureEveryNTicks);
}

void USfcSimClockSubsystem::RegisterSpawner(ASeededPropSpawner* Spawner)
{
	if (IsValid(Spawner))
	{
		RegisteredSpawners.AddUnique(Spawner);
	}
}

void USfcSimClockSubsystem::UnregisterSpawner(ASeededPropSpawner* Spawner)
{
	RegisteredSpawners.Remove(Spawner);
}

void USfcSimClockSubsystem::Tick(float DeltaTime)
{
	if (!SimClock.has_value())
	{
		return;
	}

	const double DeltaMicroseconds =
		static_cast<double>(FMath::Max(0.0f, DeltaTime)) * 1'000'000.0;
	uint64 WholeMicroseconds = static_cast<uint64>(DeltaMicroseconds);
	TickRemainderMicroseconds += DeltaMicroseconds - static_cast<double>(WholeMicroseconds);
	if (TickRemainderMicroseconds >= 1.0)
	{
		const uint64 Carry = static_cast<uint64>(TickRemainderMicroseconds);
		WholeMicroseconds += Carry;
		TickRemainderMicroseconds -= static_cast<double>(Carry);
	}

	AdvanceRealTimeMicroseconds(WholeMicroseconds);
}

TStatId USfcSimClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USfcSimClockSubsystem, STATGROUP_Tickables);
}

void USfcSimClockSubsystem::AdvanceRealTimeMicroseconds(uint64 DeltaMicroseconds)
{
	if (!SimClock.has_value())
	{
		return;
	}

	const sfc::FixedStepClock::AdvanceResult Result = SimClock->Advance(DeltaMicroseconds);
	ConsumeSimTicks(Result.ticks_ready);
}

void USfcSimClockSubsystem::ConsumeSimTicks(uint64 TickCount)
{
	for (uint64 Step = 0; Step < TickCount; ++Step)
	{
		for (ASeededPropSpawner* Spawner : RegisteredSpawners)
		{
			if (IsValid(Spawner))
			{
				Spawner->StepSim();
			}
		}

		++SimTickIndex;
		const std::optional<sfc::CaptureStamp> Stamp = CaptureTrigger.OnSimTick(SimTickIndex);
		if (Stamp.has_value())
		{
			OnCaptureTick.Broadcast(Stamp->seq, Stamp->tick_index);

			FSfcCaptureStamp Record;
			Record.Seq = static_cast<int64>(Stamp->seq);
			Record.TickIndex = static_cast<int64>(Stamp->tick_index);
			CaptureHistory.Add(Record);
		}
	}
}
