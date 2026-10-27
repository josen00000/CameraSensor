// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcSimClockSubsystem.h"
#include "sfc/wall_time_budget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSimClockSubsystemCaptureTest,
	"SFC.Scene.SimClockCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSfcSimClockSubsystemCaptureTest::RunTest(const FString& Parameters)
{
	if (!GEditor)
	{
		AddError(TEXT("GEditor is null; SimClockCapture requires an editor context."));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		AddError(TEXT("Editor world is null; SimClockCapture requires a valid UWorld."));
		return false;
	}

	USfcSimClockSubsystem* SimClock = World->GetSubsystem<USfcSimClockSubsystem>();
	if (!SimClock)
	{
		AddError(TEXT("USfcSimClockSubsystem is null on the editor world."));
		return false;
	}

	SimClock->CaptureEveryNTicks = 0;
	AddExpectedError(
		TEXT("CaptureEveryNTicks was 0"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	SimClock->ResetSimulationClock();
	TestEqual(
		TEXT("CaptureEveryNTicks clamped to 1 on reset"),
		static_cast<int32>(SimClock->CaptureEveryNTicks),
		static_cast<int32>(1));

	SimClock->CaptureEveryNTicks = 6;
	SimClock->ResetSimulationClock();

	const std::vector<uint64_t> Schedule =
		sfc::BuildAlternatingWallSchedule(sfc::kSixtySecondWallBudgetUs);
	for (uint64 DeltaMicroseconds : Schedule)
	{
		SimClock->AdvanceRealTimeMicroseconds(DeltaMicroseconds);
	}

	TestEqual(TEXT("Subsystem wall budget produces 3600 ticks"), SimClock->GetSimTickIndex(), 3600ull);

	const TArray<FSfcCaptureStamp>& History = SimClock->GetCaptureHistory();
	TestEqual(TEXT("Subsystem capture trigger count"), History.Num(), 600);
	if (History.Num() > 0)
	{
		TestEqual(TEXT("Subsystem first capture seq is 0"), History[0].Seq, 0ll);
		TestEqual(TEXT("Subsystem last capture seq is 599"), History.Last().Seq, 599ll);
	}

	return true;
}

#endif
