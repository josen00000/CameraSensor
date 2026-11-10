// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcCameraComponent.h"

USfcCameraComponent::USfcCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(this);
	SceneCapture->SetMobility(EComponentMobility::Movable);
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = SCS_FinalColorLDR;
	SceneCapture->ProjectionType = ECameraProjectionMode::Perspective;
	SceneCapture->FOVAngle = FOV;
}

void USfcCameraComponent::OnRegister()
{
	Super::OnRegister();

	if (SceneCapture && !SceneCapture->IsRegistered())
	{
		SceneCapture->RegisterComponent();
	}

	ApplyCaptureSettings();
}

void USfcCameraComponent::OnUnregister()
{
	if (SceneCapture && SceneCapture->IsRegistered())
	{
		SceneCapture->UnregisterComponent();
	}

	Super::OnUnregister();
}

#if WITH_EDITOR
void USfcCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyCaptureSettings();
}
#endif

bool USfcCameraComponent::HasValidCaptureSettings() const
{
	return ResolutionX >= 1 && ResolutionY >= 1 && FMath::IsFinite(FOV) && FOV > 0.f;
}

void USfcCameraComponent::ApplyCaptureSettings()
{
	if (!SceneCapture)
	{
		return;
	}

	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = SCS_FinalColorLDR;
	SceneCapture->ProjectionType = ECameraProjectionMode::Perspective;
	SceneCapture->FOVAngle = FOV;

	if (!HasValidCaptureSettings())
	{
		return;
	}

	const bool bTargetMatches = RenderTarget
		&& RenderTarget->SizeX == ResolutionX
		&& RenderTarget->SizeY == ResolutionY
		&& RenderTarget->GetFormat() == PF_B8G8R8A8;
	if (!bTargetMatches)
	{
		if (!RenderTarget)
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("ColorTarget"));
		}

		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->InitCustomFormat(
			static_cast<uint32>(ResolutionX),
			static_cast<uint32>(ResolutionY),
			PF_B8G8R8A8,
			false);
		RenderTarget->UpdateResourceImmediate(true);
	}

	SceneCapture->TextureTarget = RenderTarget;
}

void USfcCameraComponent::Capture()
{
	if (!SceneCapture || !GetWorld() || !HasValidCaptureSettings())
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}

	ApplyCaptureSettings();

	if (!RenderTarget || !SceneCapture->IsRegistered())
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}

	SceneCapture->CaptureScene();

	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> Read;
	const bool bRead = Resource != nullptr && Resource->ReadPixels(Read);
	const int64 ExpectedCount = static_cast<int64>(ResolutionX) * static_cast<int64>(ResolutionY);
	if (!bRead || ExpectedCount > MAX_int32 || Read.Num() != static_cast<int32>(ExpectedCount))
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}

	Pixels = MoveTemp(Read);
	bLastCaptureSucceeded = true;
}
