// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SfcCaptureTypes.generated.h"

USTRUCT(BlueprintType)
struct FSfcCaptureStamp
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SFC")
	uint64 Seq = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SFC")
	uint64 TickIndex = 0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSfcOnCaptureTick, uint64 /*Seq*/, uint64 /*TickIndex*/);
