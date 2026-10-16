// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcPropSpawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcSeededSpawnerDeterminismTest,
	"SFC.Scene.Spawner.InitialHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcSeededSpawnerDeterminismTest::RunTest(const FString& Parameters)
{
	const FVector ArenaHalfExtents(1000.0f, 1000.0f, 200.0f);

	TArray<FSfcPropState> Seed7RunA;
	TArray<FSfcPropState> Seed7RunB;
	FSfcPropSpawn::BuildStates(7, 200, ArenaHalfExtents, Seed7RunA);
	FSfcPropSpawn::BuildStates(7, 200, ArenaHalfExtents, Seed7RunB);

	const uint64 Hash7A = FSfcPropSpawn::HashInitialState(Seed7RunA);
	const uint64 Hash7B = FSfcPropSpawn::HashInitialState(Seed7RunB);
	TestEqual(TEXT("Seed 7 run A and B produce 200 props"), Seed7RunA.Num(), 200);
	TestEqual(TEXT("Seed 7 initial hash matches across two runs"), Hash7A, Hash7B);

	TArray<FSfcPropState> Seed8Run;
	FSfcPropSpawn::BuildStates(8, 200, ArenaHalfExtents, Seed8Run);
	const uint64 Hash8 = FSfcPropSpawn::HashInitialState(Seed8Run);
	TestNotEqual(TEXT("Seed 8 initial hash differs from seed 7"), Hash7A, Hash8);

	for (int32 Index = 0; Index < Seed7RunA.Num(); ++Index)
	{
		const uint16 ExpectedId = static_cast<uint16>(Index + 1);
		TestEqual(FString::Printf(TEXT("Prop ID at index %d"), Index), Seed7RunA[Index].Id, ExpectedId);
		const uint8 ClassValue = static_cast<uint8>(Seed7RunA[Index].PropClass);
		TestTrue(TEXT("Prop class is one of three"), ClassValue <= 2);
	}

	bool bClamped = false;
	const int32 Sanitized = FSfcPropSpawn::SanitizePropCount(70000, bClamped);
	TestTrue(TEXT("PropCount above 65535 is clamped"), bClamped);
	TestEqual(TEXT("Clamped prop count is 65535"), Sanitized, FSfcPropSpawn::MaxPropCount);

	TArray<FSfcPropState> ClampedRun;
	FSfcPropSpawn::BuildStates(7, 70000, ArenaHalfExtents, ClampedRun);
	TestEqual(TEXT("BuildStates respects clamp without ID wrap"), ClampedRun.Num(), FSfcPropSpawn::MaxPropCount);
	if (ClampedRun.Num() > 0)
	{
		TestEqual(TEXT("Last prop ID is 65535 when clamped"), ClampedRun.Last().Id, static_cast<uint16>(65535));
	}

	return true;
}

#endif
