// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SfcCameraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
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

	const FIntProperty* ResolutionXProperty = CastField<FIntProperty>(CameraClass->FindPropertyByName(TEXT("ResolutionX")));
	const FIntProperty* ResolutionYProperty = CastField<FIntProperty>(CameraClass->FindPropertyByName(TEXT("ResolutionY")));
	const FFloatProperty* FovProperty = CastField<FFloatProperty>(CameraClass->FindPropertyByName(TEXT("FOV")));
	TestNotNull(TEXT("ResolutionX is int32"), ResolutionXProperty);
	TestNotNull(TEXT("ResolutionY is int32"), ResolutionYProperty);
	TestNotNull(TEXT("FOV is float"), FovProperty);

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
	const FProperty* PixelsProperty = CameraClass->FindPropertyByName(TEXT("Pixels"));
	TestNotNull(TEXT("Pixels property exists"), PixelsProperty);
	TestFalse(TEXT("Pixels is not Blueprint readable"), PixelsProperty && PixelsProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	TestTrue(TEXT("bLastCaptureSucceeded is BlueprintReadOnly"), IsBlueprintReadOnly(CameraClass->FindPropertyByName(TEXT("bLastCaptureSucceeded"))));
	TestTrue(TEXT("SceneCapture is BlueprintReadOnly"), IsBlueprintReadOnly(CameraClass->FindPropertyByName(TEXT("SceneCapture"))));
	TestTrue(TEXT("RenderTarget is BlueprintReadOnly"), IsBlueprintReadOnly(CameraClass->FindPropertyByName(TEXT("RenderTarget"))));

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
	TestFalse(TEXT("Camera component does not tick"), Camera->PrimaryComponentTick.bCanEverTick);

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
	Camera->UnregisterComponent();
	TestTrue(TEXT("Unregister unregisters the scene capture"), Camera->SceneCapture && !Camera->SceneCapture->IsRegistered());
	Camera->Capture();
	RequireCleared(TEXT("Capture after unregister"));

	return true;
}

#endif
