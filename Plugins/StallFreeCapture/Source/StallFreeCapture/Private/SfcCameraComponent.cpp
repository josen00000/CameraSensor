// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcCameraComponent.h"

#include "StallFreeCapture.h"

USfcCameraComponent::USfcCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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

void USfcCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bUpdateRenderTargetEveryFrame)
	{
		return;
	}

	const ESfcCaptureReadiness Readiness = PrepareBoundRenderTarget();
	if (Readiness == ESfcCaptureReadiness::MissingRenderTarget)
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}
	if (Readiness != ESfcCaptureReadiness::Ready)
	{
		return;
	}

	SceneCapture->CaptureScene();
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

USfcCameraComponent::ESfcCaptureReadiness USfcCameraComponent::PrepareBoundRenderTarget()
{
	if (!SceneCapture || !GetWorld() || !HasValidCaptureSettings() || !SceneCapture->IsRegistered())
	{
		return ESfcCaptureReadiness::SilentFailure;
	}

	if (!SceneCapture->TextureTarget)
	{
		if (!bLoggedMissingRenderTarget)
		{
			bLoggedMissingRenderTarget = true;
			UE_LOG(LogStallFreeCapture, Error, TEXT("SfcCamera '%s' has no render target bound."), *GetName());
		}
		return ESfcCaptureReadiness::MissingRenderTarget;
	}

	bLoggedMissingRenderTarget = false;
	ApplyCaptureSettings();
	return ESfcCaptureReadiness::Ready;
}

void USfcCameraComponent::Capture()
{
	if (PrepareBoundRenderTarget() != ESfcCaptureReadiness::Ready)
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
