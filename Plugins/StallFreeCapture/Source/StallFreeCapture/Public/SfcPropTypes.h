// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SfcPropTypes.generated.h"

UENUM(BlueprintType)
enum class ESfcPropClass : uint8
{
	Cube = 0,
	Sphere = 1,
	Cylinder = 2,
};

USTRUCT(BlueprintType)
struct FSfcPropState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFC")
	uint16 Id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFC")
	ESfcPropClass PropClass = ESfcPropClass::Cube;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFC")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFC")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFC")
	FVector Scale = FVector::OneVector;
};
