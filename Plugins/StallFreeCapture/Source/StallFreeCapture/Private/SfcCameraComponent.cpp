// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcCameraComponent.h"

#include "StallFreeCapture.h"

#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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
	return ResolutionX >= 1 && ResolutionX <= MaxCaptureResolution
		&& ResolutionY >= 1 && ResolutionY <= MaxCaptureResolution
		&& FMath::IsFinite(FOV) && FOV > 0.f;
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
			RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("ColorTarget"), RF_Transient);
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

	// Leave a target assigned from outside alone. Capture() reads whatever stays bound.
	// A null target is bound here for OnRegister and property edits. Tick and Capture do not
	// reach this while the target is missing: PrepareBoundRenderTarget reports that first.
	if (!SceneCapture->TextureTarget || SceneCapture->TextureTarget == RenderTarget)
	{
		SceneCapture->TextureTarget = RenderTarget;
	}
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
	TRACE_CPUPROFILER_EVENT_SCOPE(SfcCameraCapture);

	if (PrepareBoundRenderTarget() != ESfcCaptureReadiness::Ready)
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}

	SceneCapture->CaptureScene();

	UTextureRenderTarget2D* BoundTarget = SceneCapture->TextureTarget;
	FTextureRenderTargetResource* Resource = BoundTarget ? BoundTarget->GameThread_GetRenderTargetResource() : nullptr;
	TArray<FColor> Read;
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("SfcSyncReadPixels");
	const bool bRead = Resource != nullptr && Resource->ReadPixels(Read);
	const int32 Width = BoundTarget ? BoundTarget->SizeX : 0;
	const int32 Height = BoundTarget ? BoundTarget->SizeY : 0;
	const int64 ExpectedCount = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (!bRead || Width < 1 || Height < 1 || ExpectedCount > MAX_int32 || Read.Num() != static_cast<int32>(ExpectedCount))
	{
		Pixels.Reset();
		bLastCaptureSucceeded = false;
		return;
	}

	Pixels = MoveTemp(Read);
	bLastCaptureSucceeded = true;
}

bool USfcCameraComponent::CaptureToPng(const FString& Path)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SfcCameraCaptureToPng);

	if (!IsInGameThread() || !FPaths::GetExtension(Path).Equals(TEXT("png"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	Capture();
	if (!bLastCaptureSucceeded || Pixels.IsEmpty() || !SceneCapture || !SceneCapture->TextureTarget)
	{
		return false;
	}

	const int32 Width = SceneCapture->TextureTarget->SizeX;
	const int32 Height = SceneCapture->TextureTarget->SizeY;
	const int64 ExpectedCount = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (Width < 1 || Height < 1 || ExpectedCount > MAX_int32 || Pixels.Num() != static_cast<int32>(ExpectedCount))
	{
		return false;
	}

	// Copy so the stored read keeps its alpha. File viewers treat a zero alpha as blank.
	TArray<FColor> OpaquePixels = Pixels;
	for (FColor& Pixel : OpaquePixels)
	{
		Pixel.A = 255;
	}

	const FString Directory = FPaths::GetPath(Path);
	if (!Directory.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	// Negative quality is the PNG zlib level. 9 is the slow end of this game-thread stall.
	const FImageView Image(OpaquePixels.GetData(), Width, Height, EGammaSpace::sRGB);
	return FImageUtils::SaveImageByExtension(*Path, Image, -9);
}
