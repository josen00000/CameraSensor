// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcSceneSimTestCommon.h"
#include "StallFreeCapture.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneCrossFpsTest,
	"SFC.Scene.CrossFps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneCrossFpsTest::RunTest(const FString& Parameters)
{
	const SfcSceneSimTest::WallSimRunResult Run30 =
		SfcSceneSimTest::RunSimFpsWallBudget(7, SfcSceneSimTest::kDefaultPropCount, 30);
	const SfcSceneSimTest::WallSimRunResult Run120 =
		SfcSceneSimTest::RunSimFpsWallBudget(7, SfcSceneSimTest::kDefaultPropCount, 120);

	TestEqual(
		TEXT("30 FPS wall budget feeds 60s"),
		Run30.MicrosecondsFed,
		static_cast<uint64>(sfc::kSixtySecondWallBudgetUs));
	TestEqual(
		TEXT("120 FPS wall budget feeds 60s"),
		Run120.MicrosecondsFed,
		static_cast<uint64>(sfc::kSixtySecondWallBudgetUs));
	TestEqual(TEXT("30 FPS produces exactly 3600 ticks"), Run30.TickCount, static_cast<uint64>(SfcSceneSimTest::kSimTicks));
	TestEqual(TEXT("120 FPS produces exactly 3600 ticks"), Run120.TickCount, static_cast<uint64>(SfcSceneSimTest::kSimTicks));
	TestEqual(TEXT("Tick-600 hash matches between 30 and 120 FPS framing"), Run30.HashAtTick600, Run120.HashAtTick600);

	UE_LOG(
		LogStallFreeCapture,
		Log,
		TEXT("SFC.Scene.CrossFps seed=7 tick-600 hash=%llu (30 FPS vs 120 FPS match)"),
		Run30.HashAtTick600);

	return true;
}

#endif
