// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "sfc/version.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcVersionTest,
	"StallFreeCapture.Version",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcVersionTest::RunTest(const FString& Parameters)
{
	const std::string Version = sfc::Version();
	const bool bHasValue = !Version.empty();
	TestTrue(TEXT("sfc::Version() returns a value"), bHasValue);

	const FString VersionText = UTF8_TO_TCHAR(Version.c_str());
	TestEqual(TEXT("sfc::Version()"), VersionText, FString(TEXT("0.1.0")));
	return bHasValue && VersionText == TEXT("0.1.0");
}

#endif
