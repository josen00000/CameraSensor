// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SfcCameraComponent.generated.h"

/**
 * Scene component that owns one color scene capture and the CPU pixels from its latest read.
 * Depth and an object mask can be added later as further child captures and buffers.
 */
UCLASS(ClassGroup = (SFC), meta = (BlueprintSpawnableComponent))
class STALLFREECAPTURE_API USfcCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USfcCameraComponent();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SFC|Camera")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SFC|Camera")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFC|Camera", meta = (ClampMin = "1"))
	int32 ResolutionX = 1280;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFC|Camera", meta = (ClampMin = "1"))
	int32 ResolutionY = 720;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SFC|Camera", meta = (ClampMin = "0.001", ClampMax = "360.0"))
	float FOV = 90.f;

	/** CPU image from the last capture. C++ only: a 1280x720 buffer is too large for Blueprint or the details panel. */
	UPROPERTY(Transient)
	TArray<FColor> Pixels;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SFC|Camera")
	bool bLastCaptureSucceeded = false;

	/** Capture once into the owned render target, then ReadPixels into Pixels. */
	UFUNCTION(BlueprintCallable, Category = "SFC|Camera")
	void Capture();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool HasValidCaptureSettings() const;
	void ApplyCaptureSettings();
};
