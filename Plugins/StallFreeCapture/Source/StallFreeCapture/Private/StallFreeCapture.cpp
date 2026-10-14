// Copyright Epic Games, Inc. All Rights Reserved.

#include "StallFreeCapture.h"
#include "Interfaces/IPluginManager.h"
#include "sfc/version.h"

#define LOCTEXT_NAMESPACE "FStallFreeCaptureModule"

DEFINE_LOG_CATEGORY(LogStallFreeCapture);

void FStallFreeCaptureModule::StartupModule()
{
	FString Version = TEXT("unknown");
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StallFreeCapture"));
	if (Plugin.IsValid())
	{
		Version = Plugin->GetDescriptor().VersionName;
	}

	UE_LOG(LogStallFreeCapture, Log, TEXT("StallFreeCapture loaded %s"), *Version);
	UE_LOG(LogStallFreeCapture, Log, TEXT("sfc_core %s"), UTF8_TO_TCHAR(sfc::Version().c_str()));
}

void FStallFreeCaptureModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStallFreeCaptureModule, StallFreeCapture)
