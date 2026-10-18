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
	TArray<uint64> DirectHashes;
	TestTrue(
		TEXT("Direct run produced 3600 hashes"),
		SfcSceneSimTest::RunPerTickHashesDirect(7, SfcSceneSimTest::kDefaultPropCount, DirectHashes));

	TArray<uint64> HitchHashes;
	const bool bHitchOk = SfcSceneSimTest::RunPerTickHashes(
		7,
		SfcSceneSimTest::kDefaultPropCount,
		HitchHashes,
		true);
	TestTrue(TEXT("Hitch run produced 3600 hashes"), bHitchOk);

	if (DirectHashes.Num() == SfcSceneSimTest::kSimTicks && HitchHashes.Num() == SfcSceneSimTest::kSimTicks)
	{
		const int32 Mismatch = SfcSceneSimTest::FindFirstMismatchingTick(DirectHashes, HitchHashes);
		TestEqual(TEXT("Tick-3600 hash matches between hitch and no-hitch runs"), Mismatch, -1);
		if (Mismatch != -1)
		{
			AddError(FString::Printf(TEXT("First mismatching tick: %d"), Mismatch));
		}
	}

	return true;
}

#endif
