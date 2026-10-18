// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcSceneSimTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneSeeds20Test,
	"SFC.Scene.Seeds20",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneSeeds20Test::RunTest(const FString& Parameters)
{
	int32 Failures = 0;
	for (uint64 Seed = 1; Seed <= 20; ++Seed)
	{
		TArray<uint64> RunA;
		TArray<uint64> RunB;
		if (!SfcSceneSimTest::RunPerTickHashesDirect(Seed, SfcSceneSimTest::kDefaultPropCount, RunA) ||
			!SfcSceneSimTest::RunPerTickHashesDirect(Seed, SfcSceneSimTest::kDefaultPropCount, RunB))
		{
			++Failures;
			continue;
		}

		const int32 Mismatch = SfcSceneSimTest::FindFirstMismatchingTick(RunA, RunB);
		if (Mismatch != -1)
		{
			AddError(FString::Printf(TEXT("Seed %llu mismatch at tick %d"), Seed, Mismatch));
			++Failures;
		}
	}

	TestEqual(TEXT("Seeds 1..20 have zero same-seed failures"), Failures, 0);
	return Failures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneHitchTest,
	"SFC.Scene.Hitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneHitchTest::RunTest(const FString& Parameters)
{
	const SfcSceneSimTest::WallSimRunResult NoHitch =
		SfcSceneSimTest::RunSimSixtySecondWallBudget(7, SfcSceneSimTest::kDefaultPropCount, false);
	const SfcSceneSimTest::WallSimRunResult WithHitch =
		SfcSceneSimTest::RunSimSixtySecondWallBudget(7, SfcSceneSimTest::kDefaultPropCount, true);

	TestEqual(
		TEXT("No-hitch wall budget feeds 60s"),
		NoHitch.MicrosecondsFed,
		static_cast<uint64>(sfc::kSixtySecondWallBudgetUs));
	TestEqual(
		TEXT("Hitch wall budget feeds 60s"),
		WithHitch.MicrosecondsFed,
		static_cast<uint64>(sfc::kSixtySecondWallBudgetUs));
	TestEqual(TEXT("No-hitch produces exactly 3600 ticks"), NoHitch.TickCount, static_cast<uint64>(SfcSceneSimTest::kSimTicks));
	TestEqual(TEXT("Hitch produces exactly 3600 ticks"), WithHitch.TickCount, static_cast<uint64>(SfcSceneSimTest::kSimTicks));

	TestEqual(TEXT("Tick-3600 hash matches between hitch and no-hitch runs"), NoHitch.HashAtTick3600, WithHitch.HashAtTick3600);

	TestEqual(TEXT("Capture trigger count"), NoHitch.CaptureHistory.Num(), 600);
	TestEqual(TEXT("Hitch capture list length matches"), WithHitch.CaptureHistory.Num(), NoHitch.CaptureHistory.Num());
	for (int32 Index = 0; Index < FMath::Min(NoHitch.CaptureHistory.Num(), WithHitch.CaptureHistory.Num()); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Capture seq at index %d"), Index), WithHitch.CaptureHistory[Index].Seq, NoHitch.CaptureHistory[Index].Seq);
		TestEqual(FString::Printf(TEXT("Capture tick at index %d"), Index), WithHitch.CaptureHistory[Index].TickIndex, NoHitch.CaptureHistory[Index].TickIndex);
	}

	return true;
}

#endif
