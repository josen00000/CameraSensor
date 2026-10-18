// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcCaptureTypes.h"
#include "SfcSceneSimTestCommon.h"
#include "sfc/capture_trigger.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSceneCaptureTriggerTest,
	"SFC.Scene.CaptureTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSceneCaptureTriggerTest::RunTest(const FString& Parameters)
{
	const SfcSceneSimTest::WallSimRunResult NoHitch =
		SfcSceneSimTest::RunSimSixtySecondWallBudget(7, SfcSceneSimTest::kDefaultPropCount, false);
	const SfcSceneSimTest::WallSimRunResult WithHitch =
		SfcSceneSimTest::RunSimSixtySecondWallBudget(7, SfcSceneSimTest::kDefaultPropCount, true);

	TestEqual(TEXT("Wall budget produces exactly 3600 ticks"), NoHitch.TickCount, static_cast<uint64>(SfcSceneSimTest::kSimTicks));
	TestEqual(TEXT("Capture trigger count over 3600 ticks"), NoHitch.CaptureHistory.Num(), 600);
	if (NoHitch.CaptureHistory.Num() > 0)
	{
		TestEqual(TEXT("First capture seq is 0"), NoHitch.CaptureHistory[0].Seq, 0ll);
		TestEqual(TEXT("First capture tick is 6"), NoHitch.CaptureHistory[0].TickIndex, 6ll);
		TestEqual(TEXT("Last capture seq is 599"), NoHitch.CaptureHistory.Last().Seq, 599ll);
		TestEqual(TEXT("Last capture tick is 3600"), NoHitch.CaptureHistory.Last().TickIndex, 3600ll);
	}

	TestEqual(TEXT("Hitch capture list length matches"), WithHitch.CaptureHistory.Num(), NoHitch.CaptureHistory.Num());
	for (int32 Index = 0; Index < FMath::Min(NoHitch.CaptureHistory.Num(), WithHitch.CaptureHistory.Num()); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Capture seq at index %d"), Index), WithHitch.CaptureHistory[Index].Seq, NoHitch.CaptureHistory[Index].Seq);
		TestEqual(FString::Printf(TEXT("Capture tick at index %d"), Index), WithHitch.CaptureHistory[Index].TickIndex, NoHitch.CaptureHistory[Index].TickIndex);
	}

	bool bClamped = false;
	const uint32 SafeN = sfc::SanitizeCaptureEveryNTicks(0, bClamped);
	TestTrue(TEXT("CaptureEveryNTicks 0 is clamped"), bClamped);
	TestEqual(TEXT("CaptureEveryNTicks clamps to 1"), SafeN, 1u);

	return true;
}

#endif
