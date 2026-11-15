// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcCameraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
	struct FScopedGameWorld
	{
		UWorld* World = nullptr;

		FScopedGameWorld()
		{
			if (!GEngine)
			{
				return;
			}

			const FName WorldName = MakeUniqueObjectName(
				nullptr,
				UWorld::StaticClass(),
				TEXT("SfcCameraTest"),
				EUniqueObjectNameOptions::GloballyUnique);
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			if (!World)
			{
				return;
			}

			World->AddToRoot();
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedGameWorld()
		{
			if (!World || !GEngine)
			{
				return;
			}

			if (World->AreActorsInitialized())
			{
				for (FActorIterator It(World); It; ++It)
				{
					It->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}

			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			World->SetPhysicsScene(nullptr);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
			World = nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSfcCameraReadPixelsTest,
	"StallFreeCapture.Camera.ReadPixels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSfcCameraReadPixelsTest::RunTest(const FString& Parameters)
{
	UClass* CameraClass = USfcCameraComponent::StaticClass();
	UFunction* CaptureFunction = CameraClass->FindFunctionByName(TEXT("Capture"));
	TestNotNull(TEXT("Capture is a UFunction"), CaptureFunction);
	if (CaptureFunction)
	{
		TestTrue(TEXT("Capture is BlueprintCallable"), CaptureFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}

	UFunction* CaptureToPngFunction = CameraClass->FindFunctionByName(TEXT("CaptureToPng"));
	TestNotNull(TEXT("CaptureToPng is a UFunction"), CaptureToPngFunction);
	if (CaptureToPngFunction)
	{
		TestTrue(TEXT("CaptureToPng is BlueprintCallable"), CaptureToPngFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
	}

	const FIntProperty* ResolutionXProperty = CastField<FIntProperty>(CameraClass->FindPropertyByName(TEXT("ResolutionX")));
	const FIntProperty* ResolutionYProperty = CastField<FIntProperty>(CameraClass->FindPropertyByName(TEXT("ResolutionY")));
	const FFloatProperty* FovProperty = CastField<FFloatProperty>(CameraClass->FindPropertyByName(TEXT("FOV")));
	const FBoolProperty* UpdateEveryFrameProperty = CastField<FBoolProperty>(CameraClass->FindPropertyByName(TEXT("bUpdateRenderTargetEveryFrame")));
	TestNotNull(TEXT("ResolutionX is int32"), ResolutionXProperty);
	TestNotNull(TEXT("ResolutionY is int32"), ResolutionYProperty);
	TestNotNull(TEXT("FOV is float"), FovProperty);
	TestNotNull(TEXT("bUpdateRenderTargetEveryFrame is bool"), UpdateEveryFrameProperty);

	auto IsBlueprintReadWrite = [](const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			&& !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
	};
	auto IsBlueprintReadOnly = [](const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			&& Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly);
	};

	TestTrue(TEXT("ResolutionX is BlueprintReadWrite"), IsBlueprintReadWrite(ResolutionXProperty));
	TestTrue(TEXT("ResolutionY is BlueprintReadWrite"), IsBlueprintReadWrite(ResolutionYProperty));
	TestTrue(TEXT("FOV is BlueprintReadWrite"), IsBlueprintReadWrite(FovProperty));
	TestTrue(TEXT("bUpdateRenderTargetEveryFrame is BlueprintReadWrite"), IsBlueprintReadWrite(UpdateEveryFrameProperty));
	const FProperty* PixelsProperty = CameraClass->FindPropertyByName(TEXT("Pixels"));
	TestNotNull(TEXT("Pixels property exists"), PixelsProperty);
	TestFalse(TEXT("Pixels is not Blueprint readable"), PixelsProperty && PixelsProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	TestTrue(TEXT("bLastCaptureSucceeded is BlueprintReadOnly"), IsBlueprintReadOnly(CameraClass->FindPropertyByName(TEXT("bLastCaptureSucceeded"))));
	TestTrue(TEXT("SceneCapture is BlueprintReadOnly"), IsBlueprintReadOnly(CameraClass->FindPropertyByName(TEXT("SceneCapture"))));
	const FProperty* RenderTargetProperty = CameraClass->FindPropertyByName(TEXT("RenderTarget"));
	TestTrue(TEXT("RenderTarget is BlueprintReadOnly"), IsBlueprintReadOnly(RenderTargetProperty));
	TestTrue(TEXT("RenderTarget is transient"), RenderTargetProperty && RenderTargetProperty->HasAnyPropertyFlags(CPF_Transient));
	TestEqual(TEXT("ResolutionX ClampMax is 8192"), ResolutionXProperty ? ResolutionXProperty->GetMetaData(TEXT("ClampMax")) : FString(), FString(TEXT("8192")));
	TestEqual(TEXT("ResolutionY ClampMax is 8192"), ResolutionYProperty ? ResolutionYProperty->GetMetaData(TEXT("ClampMax")) : FString(), FString(TEXT("8192")));
	TestEqual(TEXT("MaxCaptureResolution matches ClampMax"), USfcCameraComponent::MaxCaptureResolution, 8192);

	if (!FApp::CanEverRender())
	{
		AddError(TEXT("This test reads a render target, so it needs a rendering RHI. -nullrhi does not create one."));
		return false;
	}

	if (!GEngine)
	{
		AddError(TEXT("GEngine is null; camera capture requires an engine."));
		return false;
	}

	FScopedGameWorld ScopedWorld;
	UWorld* World = ScopedWorld.World;
	if (!World)
	{
		AddError(TEXT("Failed to create a world for the camera capture test."));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto SpawnHost = [&](const TCHAR* What) -> AActor*
	{
		AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!Host)
		{
			AddError(FString::Printf(TEXT("Failed to spawn an actor for %s."), What));
		}
		return Host;
	};

	AActor* IdleActor = SpawnHost(TEXT("the unregistered camera"));
	AActor* Actor = SpawnHost(TEXT("the camera component"));
	AActor* OtherActor = SpawnHost(TEXT("the second camera"));
	if (!IdleActor || !Actor || !OtherActor)
	{
		return false;
	}

	USfcCameraComponent* Camera = NewObject<USfcCameraComponent>(Actor, TEXT("Camera"));
	Actor->SetRootComponent(Camera);
	Camera->RegisterComponent();
	TestTrue(TEXT("Camera component ticks"), Camera->PrimaryComponentTick.bCanEverTick);
	TestTrue(TEXT("Render target updates every frame by default"), Camera->bUpdateRenderTargetEveryFrame);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* EmissiveMaterial = GEngine ? GEngine->EmissiveMeshMaterial : nullptr;
	TestNotNull(TEXT("Engine cube mesh loads"), CubeMesh);
	TestNotNull(TEXT("Emissive mesh material exists"), EmissiveMaterial);
	AActor* PropActor = SpawnHost(TEXT("the emissive cube"));
	if (!CubeMesh || !EmissiveMaterial || !PropActor)
	{
		return false;
	}

	UStaticMeshComponent* Cube = NewObject<UStaticMeshComponent>(PropActor, TEXT("Cube"));
	PropActor->SetRootComponent(Cube);
	Cube->SetStaticMesh(CubeMesh);
	Cube->SetMaterial(0, EmissiveMaterial);
	Cube->SetWorldLocation(FVector(250.f, 0.f, 0.f));
	Cube->SetWorldScale3D(FVector(4.f));
	Cube->RegisterComponent();
	Camera->SceneCapture->ShowFlags.SetAtmosphere(false);
	Camera->SceneCapture->ShowFlags.SetFog(false);
	Camera->SceneCapture->ShowFlags.SetEyeAdaptation(false);
	Camera->SceneCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->SceneCapture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->SceneCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->SceneCapture->PostProcessSettings.AutoExposureBias = 1.f;
	World->SendAllEndOfFrameUpdates();

	USfcCameraComponent* IdleCamera = NewObject<USfcCameraComponent>(IdleActor, TEXT("IdleCamera"));
	IdleActor->SetRootComponent(IdleCamera);
	IdleCamera->Capture();
	TestFalse(TEXT("Capture before register fails"), IdleCamera->bLastCaptureSucceeded);
	TestEqual(TEXT("Capture before register leaves pixels empty"), IdleCamera->Pixels.Num(), 0);
	TestTrue(TEXT("Scene capture stays unregistered until RegisterComponent"), IdleCamera->SceneCapture && !IdleCamera->SceneCapture->IsRegistered());

	USfcCameraComponent* OtherCamera = NewObject<USfcCameraComponent>(OtherActor, TEXT("OtherCamera"));
	OtherActor->SetRootComponent(OtherCamera);
	OtherCamera->RegisterComponent();
	TestNotNull(TEXT("Second camera owns a render target"), OtherCamera->RenderTarget.Get());
	TestNotEqual(TEXT("Two cameras own two render targets"), Camera->RenderTarget.Get(), OtherCamera->RenderTarget.Get());
	TestNotEqual(TEXT("Two cameras own two scene captures"), Camera->SceneCapture.Get(), OtherCamera->SceneCapture.Get());

	for (int32 TickIndex = 0; TickIndex < 3; ++TickIndex)
	{
		Camera->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->PrimaryComponentTick);
		Camera->SceneCapture->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->SceneCapture->PrimaryComponentTick);
	}
	TestFalse(TEXT("Component ticks do not mark a capture as succeeded"), Camera->bLastCaptureSucceeded);
	TestEqual(TEXT("Component ticks do not store pixels"), Camera->Pixels.Num(), 0);
	TestFalse(TEXT("Scene capture stays off every frame after ticks"), Camera->SceneCapture->bCaptureEveryFrame);

	TestNotNull(TEXT("Scene capture child exists"), Camera->SceneCapture.Get());
	if (!Camera->SceneCapture)
	{
		return false;
	}

	TestTrue(TEXT("Scene capture is registered"), Camera->SceneCapture->IsRegistered());
	TestEqual(TEXT("Scene capture is attached to the camera component"), Camera->SceneCapture->GetAttachParent(), static_cast<USceneComponent*>(Camera));
	TestFalse(TEXT("Scene capture does not capture every frame"), Camera->SceneCapture->bCaptureEveryFrame);
	TestFalse(TEXT("Scene capture does not capture on movement"), Camera->SceneCapture->bCaptureOnMovement);
	TestEqual(TEXT("Capture source is final color LDR"), Camera->SceneCapture->CaptureSource, TEnumAsByte<ESceneCaptureSource>(SCS_FinalColorLDR));
	TestEqual(TEXT("Projection is perspective"), Camera->SceneCapture->ProjectionType, TEnumAsByte<ECameraProjectionMode::Type>(ECameraProjectionMode::Perspective));
	TestEqual(TEXT("Default FOV is 90"), Camera->SceneCapture->FOVAngle, 90.f);
	TestNotNull(TEXT("Transient render target exists"), Camera->RenderTarget.Get());
	if (!Camera->RenderTarget)
	{
		return false;
	}

	UTextureRenderTarget2D* OwnedTarget = Camera->RenderTarget;
	TestEqual(TEXT("Render target width is 1280"), OwnedTarget->SizeX, 1280);
	TestEqual(TEXT("Render target height is 720"), OwnedTarget->SizeY, 720);
	TestEqual(TEXT("Render target format is PF_B8G8R8A8"), OwnedTarget->GetFormat(), PF_B8G8R8A8);
	TestTrue(TEXT("Scene capture writes the owned render target"), Camera->SceneCapture->TextureTarget == OwnedTarget);

	auto RequirePixels = [&](int32 Width, int32 Height, const TCHAR* What)
	{
		TestTrue(What, Camera->bLastCaptureSucceeded);
		TestEqual(FString::Printf(TEXT("%s stores %dx%d colors"), What, Width, Height), Camera->Pixels.Num(), Width * Height);
	};
	auto RequireCleared = [&](const TCHAR* What)
	{
		TestFalse(What, Camera->bLastCaptureSucceeded);
		TestEqual(FString::Printf(TEXT("%s clears pixels"), What), Camera->Pixels.Num(), 0);
	};

	Camera->Capture();
	RequirePixels(1280, 720, TEXT("Default capture"));

	auto CountNonBlack = [](const TArray<FColor>& Colors)
	{
		int32 Count = 0;
		for (const FColor& Color : Colors)
		{
			if ((Color.R | Color.G | Color.B) != 0)
			{
				++Count;
			}
		}
		return Count;
	};
	const int32 NonBlackPixels = CountNonBlack(Camera->Pixels);
	TestTrue(TEXT("Default capture is not a black frame"), NonBlackPixels > 1000);

	const FString PngPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SfcCameraTest"), TEXT("sync-capture.png"));
	TestFalse(TEXT("CaptureToPng rejects an empty path"), Camera->CaptureToPng(TEXT("")));
	TestFalse(TEXT("CaptureToPng rejects a non-png path"), Camera->CaptureToPng(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SfcCameraTest"), TEXT("sync-capture.jpg"))));
	TestEqual(TEXT("A rejected PNG path keeps the last pixel count"), Camera->Pixels.Num(), 1280 * 720);
	const double PngStartSeconds = FPlatformTime::Seconds();
	TestTrue(TEXT("CaptureToPng writes a file"), Camera->CaptureToPng(PngPath));
	AddInfo(FString::Printf(TEXT("CaptureToPng 1280x720 took %.2f ms on the game thread"), (FPlatformTime::Seconds() - PngStartSeconds) * 1000.0));
	TestTrue(TEXT("PNG file exists"), FPaths::FileExists(PngPath));

	FImage DecodedPng;
	TestTrue(TEXT("PNG loads"), FImageUtils::LoadImage(*PngPath, DecodedPng));
	TestEqual(TEXT("PNG width is 1280"), DecodedPng.SizeX, 1280);
	TestEqual(TEXT("PNG height is 720"), DecodedPng.SizeY, 720);
	TestEqual(TEXT("PNG is BGRA8"), DecodedPng.Format, ERawImageFormat::BGRA8);
	if (DecodedPng.Format == ERawImageFormat::BGRA8)
	{
		const TArrayView64<FColor> DecodedColors = DecodedPng.AsBGRA8();
		TestEqual(TEXT("PNG pixel count matches Pixels"), static_cast<int32>(DecodedColors.Num()), Camera->Pixels.Num());
		bool bPixelsMatch = DecodedColors.Num() == Camera->Pixels.Num();
		int32 DecodedNonBlack = 0;
		for (int32 PixelIndex = 0; bPixelsMatch && PixelIndex < DecodedColors.Num(); ++PixelIndex)
		{
			const FColor& FilePixel = DecodedColors[PixelIndex];
			const FColor& CapturedPixel = Camera->Pixels[PixelIndex];
			if ((FilePixel.R | FilePixel.G | FilePixel.B) != 0)
			{
				++DecodedNonBlack;
			}
			bPixelsMatch = FilePixel.R == CapturedPixel.R
				&& FilePixel.G == CapturedPixel.G
				&& FilePixel.B == CapturedPixel.B
				&& FilePixel.A == 255;
		}
		TestTrue(TEXT("PNG RGB matches Pixels and alpha is opaque"), bPixelsMatch);
		TestTrue(TEXT("PNG is not a black frame"), DecodedNonBlack > 1000);
	}

	Camera->SceneCapture->bCaptureEveryFrame = true;
	Camera->SceneCapture->bCaptureOnMovement = true;
	Camera->SceneCapture->CaptureSource = SCS_SceneColorHDR;
	Camera->SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->SceneCapture->FOVAngle = 15.f;
	Camera->Capture();
	TestFalse(TEXT("Capture forces every-frame capture off"), Camera->SceneCapture->bCaptureEveryFrame);
	TestFalse(TEXT("Capture forces on-movement capture off"), Camera->SceneCapture->bCaptureOnMovement);
	TestEqual(TEXT("Capture forces final color LDR"), Camera->SceneCapture->CaptureSource, TEnumAsByte<ESceneCaptureSource>(SCS_FinalColorLDR));
	TestEqual(TEXT("Capture forces perspective"), Camera->SceneCapture->ProjectionType, TEnumAsByte<ECameraProjectionMode::Type>(ECameraProjectionMode::Perspective));
	TestEqual(TEXT("Capture restores the component FOV"), Camera->SceneCapture->FOVAngle, Camera->FOV);
	TestEqual(TEXT("Forcing settings keeps the same render target"), Camera->RenderTarget.Get(), OwnedTarget);
	RequirePixels(1280, 720, TEXT("Capture after restoring settings"));

	const FVector Location(100.f, -40.f, 25.f);
	const FRotator Rotation(10.f, 45.f, 5.f);
	Camera->SetWorldLocationAndRotation(Location, Rotation);
	const FTransform ChildTransform = Camera->SceneCapture->GetComponentTransform();
	TestTrue(TEXT("Scene capture follows the camera location"), ChildTransform.GetLocation().Equals(Location, 0.01));
	TestTrue(TEXT("Scene capture follows the camera rotation"), ChildTransform.Rotator().Equals(Rotation, 0.01f));
	TestEqual(TEXT("Moving the camera does not replace stored pixels"), Camera->Pixels.Num(), 1280 * 720);

	Camera->FOV = 60.f;
	Camera->ResolutionX = 320;
	Camera->ResolutionY = 180;
	Camera->Capture();
	TestEqual(TEXT("Capture pushes FOV onto the scene capture"), Camera->SceneCapture->FOVAngle, 60.f);
	TestEqual(TEXT("Resize keeps the same render target"), Camera->RenderTarget.Get(), OwnedTarget);
	TestEqual(TEXT("Capture resizes render target width"), Camera->RenderTarget->SizeX, 320);
	TestEqual(TEXT("Capture resizes render target height"), Camera->RenderTarget->SizeY, 180);
	TestEqual(TEXT("Resized render target stays PF_B8G8R8A8"), Camera->RenderTarget->GetFormat(), PF_B8G8R8A8);
	TestEqual(TEXT("Scene capture still writes the owned render target"), Camera->SceneCapture->TextureTarget, Camera->RenderTarget);
	RequirePixels(320, 180, TEXT("Resized capture"));

	auto RestoreSmallCapture = [&]()
	{
		Camera->ResolutionX = 64;
		Camera->ResolutionY = 64;
		Camera->FOV = 90.f;
		Camera->Capture();
		RequirePixels(64, 64, TEXT("Restored capture"));
	};

	RestoreSmallCapture();
	const int32 TargetWidthBeforeFailure = Camera->RenderTarget->SizeX;
	const int32 TargetHeightBeforeFailure = Camera->RenderTarget->SizeY;
	Camera->ResolutionX = 0;
	Camera->Capture();
	RequireCleared(TEXT("ResolutionX of 0"));
	TestEqual(TEXT("Invalid resolution keeps the previous target width"), Camera->RenderTarget->SizeX, TargetWidthBeforeFailure);
	TestEqual(TEXT("Invalid resolution keeps the previous target height"), Camera->RenderTarget->SizeY, TargetHeightBeforeFailure);

	RestoreSmallCapture();
	Camera->ResolutionY = -1;
	Camera->Capture();
	RequireCleared(TEXT("Negative ResolutionY"));

	RestoreSmallCapture();
	Camera->FOV = 0.f;
	Camera->Capture();
	RequireCleared(TEXT("FOV of 0"));

	RestoreSmallCapture();
	Camera->FOV = std::numeric_limits<float>::quiet_NaN();
	Camera->Capture();
	RequireCleared(TEXT("Non-finite FOV"));

	RestoreSmallCapture();
	const int32 TargetWidthBeforeMax = Camera->RenderTarget->SizeX;
	const int32 TargetHeightBeforeMax = Camera->RenderTarget->SizeY;
	Camera->ResolutionX = USfcCameraComponent::MaxCaptureResolution + 1;
	Camera->Capture();
	RequireCleared(TEXT("ResolutionX above the maximum"));
	TestEqual(TEXT("Oversized resolution keeps the previous target width"), Camera->RenderTarget->SizeX, TargetWidthBeforeMax);
	TestEqual(TEXT("Oversized resolution keeps the previous target height"), Camera->RenderTarget->SizeY, TargetHeightBeforeMax);

	RestoreSmallCapture();
	Camera->bUpdateRenderTargetEveryFrame = false;
	Camera->SceneCapture->TextureTarget = nullptr;
	Camera->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->PrimaryComponentTick);
	TestTrue(TEXT("A disabled frame update does not clear the last capture"), Camera->bLastCaptureSucceeded);
	TestEqual(TEXT("A disabled frame update does not clear stored pixels"), Camera->Pixels.Num(), 64 * 64);
	TestNull(TEXT("Disabled frame update leaves an unbound render target unbound"), Camera->SceneCapture->TextureTarget.Get());

	Camera->bUpdateRenderTargetEveryFrame = true;
	AddExpectedError(TEXT("has no render target bound"), EAutomationExpectedErrorFlags::Contains, 2);
	Camera->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->PrimaryComponentTick);
	Camera->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->PrimaryComponentTick);
	Camera->Capture();
	RequireCleared(TEXT("Missing render target"));
	TestNull(TEXT("Missing render target stays unbound"), Camera->SceneCapture->TextureTarget.Get());
	TestNotNull(TEXT("Owned render target is kept while unbound"), Camera->RenderTarget.Get());

	Camera->SceneCapture->TextureTarget = Camera->RenderTarget;
	Camera->Capture();
	RequirePixels(64, 64, TEXT("Rebinding the render target"));

	UTextureRenderTarget2D* ExternalTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	ExternalTarget->InitCustomFormat(32, 32, PF_B8G8R8A8, false);
	ExternalTarget->UpdateResourceImmediate(true);
	Camera->SceneCapture->TextureTarget = ExternalTarget;
	Camera->Capture();
	TestEqual(TEXT("An external render target stays bound"), Camera->SceneCapture->TextureTarget.Get(), ExternalTarget);
	RequirePixels(32, 32, TEXT("Capture from an external render target"));
	TestNotEqual(TEXT("The owned render target is not the external one"), Camera->RenderTarget.Get(), ExternalTarget);
	Camera->TickComponent(1.f / 60.f, LEVELTICK_All, &Camera->PrimaryComponentTick);
	TestEqual(TEXT("A tick does not replace an external render target"), Camera->SceneCapture->TextureTarget.Get(), ExternalTarget);

	Camera->SceneCapture->TextureTarget = nullptr;
	Camera->Capture();
	RequireCleared(TEXT("Missing render target logs again after a rebind"));
	TestNull(TEXT("Second missing render target stays unbound"), Camera->SceneCapture->TextureTarget.Get());

	Camera->SceneCapture->TextureTarget = Camera->RenderTarget;
	Camera->UnregisterComponent();
	TestTrue(TEXT("Unregister unregisters the scene capture"), Camera->SceneCapture && !Camera->SceneCapture->IsRegistered());
	Camera->Capture();
	RequireCleared(TEXT("Capture after unregister"));

	return true;
}

#endif
