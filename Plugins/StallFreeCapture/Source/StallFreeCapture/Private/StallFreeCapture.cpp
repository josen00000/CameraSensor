// Copyright Epic Games, Inc. All Rights Reserved.

#include "StallFreeCapture.h"
#include "Interfaces/IPluginManager.h"

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
}

void FStallFreeCaptureModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStallFreeCaptureModule, StallFreeCapture)
