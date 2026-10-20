// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcSceneSimTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneDeterminismTest,
	"SFC.Scene.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneDeterminismTest::RunTest(const FString& Parameters)
{
	TArray<uint64> Seed7RunA;
	TArray<uint64> Seed7RunB;
	TestTrue(
		TEXT("Seed 7 run A produced 3600 tick hashes"),
		SfcSceneSimTest::RunPerTickHashesDirect(7, SfcSceneSimTest::kDefaultPropCount, Seed7RunA));
	TestTrue(
		TEXT("Seed 7 run B produced 3600 tick hashes"),
		SfcSceneSimTest::RunPerTickHashesDirect(7, SfcSceneSimTest::kDefaultPropCount, Seed7RunB));

	const int32 Mismatch7 = SfcSceneSimTest::FindFirstMismatchingTick(Seed7RunA, Seed7RunB);
	TestEqual(TEXT("Seed 7 per-tick hashes match across two runs"), Mismatch7, -1);

	TArray<uint64> Seed8Run;
	TestTrue(
		TEXT("Seed 8 produced 3600 tick hashes"),
		SfcSceneSimTest::RunPerTickHashesDirect(8, SfcSceneSimTest::kDefaultPropCount, Seed8Run));
	if (Seed7RunA.Num() > 0 && Seed8Run.Num() > 0)
	{
		TestNotEqual(TEXT("Seed 8 differs from seed 7 at tick 1"), Seed8Run[0], Seed7RunA[0]);
	}

	if (Mismatch7 != -1)
	{
		AddError(FString::Printf(TEXT("First mismatching tick: %d"), Mismatch7));
	}

	return true;
}

#endif
