// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcCaptureTypes.h"
#include "SfcSimClockSubsystem.h"
#include "sfc/capture_trigger.h"
#include "sfc/fixed_step_clock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 kSimTicks = 3600;

	TArray<FSfcCaptureStamp> RunCaptureHistory(bool bWithHitch)
	{
		TArray<FSfcCaptureStamp> History;

		sfc::CaptureTrigger Trigger;
		Trigger.Reset(6);

		auto clock = sfc::FixedStepClock::Create(
			USfcSimClockSubsystem::kSimPeriodMicroseconds,
			USfcSimClockSubsystem::kSimTicksPerSecond);
		uint64 sim_ticks = 0;
		while (sim_ticks < kSimTicks)
		{
			uint64 delta_us = 16'667;
			if (bWithHitch && sim_ticks == 999)
			{
				delta_us = 200'000;
			}
			const auto advance = clock->Advance(delta_us);
			for (uint64 step = 0; step < advance.ticks_ready; ++step)
			{
				++sim_ticks;
				const std::optional<sfc::CaptureStamp> stamp = Trigger.OnSimTick(sim_ticks);
				if (stamp.has_value())
				{
					FSfcCaptureStamp Record;
					Record.Seq = stamp->seq;
					Record.TickIndex = stamp->tick_index;
					History.Add(Record);
				}
				if (sim_ticks >= kSimTicks)
				{
					break;
				}
			}
		}
		return History;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneCaptureTriggerTest,
	"SFC.Scene.CaptureTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneCaptureTriggerTest::RunTest(const FString& Parameters)
{
	const TArray<FSfcCaptureStamp> NoHitch = RunCaptureHistory(false);
	const TArray<FSfcCaptureStamp> WithHitch = RunCaptureHistory(true);

	TestEqual(TEXT("Capture trigger count over 3600 ticks"), NoHitch.Num(), 600);
	if (NoHitch.Num() > 0)
	{
		TestEqual(TEXT("First capture seq is 0"), NoHitch[0].Seq, 0ull);
		TestEqual(TEXT("First capture tick is 6"), NoHitch[0].TickIndex, 6ull);
		TestEqual(TEXT("Last capture seq is 599"), NoHitch.Last().Seq, 599ull);
		TestEqual(TEXT("Last capture tick is 3600"), NoHitch.Last().TickIndex, 3600ull);
	}

	TestEqual(TEXT("Hitch capture list length matches"), WithHitch.Num(), NoHitch.Num());
	for (int32 Index = 0; Index < FMath::Min(NoHitch.Num(), WithHitch.Num()); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Capture seq at index %d"), Index), WithHitch[Index].Seq, NoHitch[Index].Seq);
		TestEqual(FString::Printf(TEXT("Capture tick at index %d"), Index), WithHitch[Index].TickIndex, NoHitch[Index].TickIndex);
	}

	bool bClamped = false;
	const uint32 SafeN = sfc::SanitizeCaptureEveryNTicks(0, bClamped);
	TestTrue(TEXT("CaptureEveryNTicks 0 is clamped"), bClamped);
	TestEqual(TEXT("CaptureEveryNTicks clamps to 1"), SafeN, 1u);

	return true;
}

#endif
