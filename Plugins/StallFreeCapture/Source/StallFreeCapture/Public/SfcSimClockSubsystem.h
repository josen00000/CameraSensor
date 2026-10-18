// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SfcCaptureTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "sfc/capture_trigger.h"
#include "sfc/fixed_step_clock.h"
#include "SfcSimClockSubsystem.generated.h"

class ASeededPropSpawner;

UCLASS()
class STALLFREECAPTURE_API USfcSimClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr uint64 kSimPeriodMicroseconds = 1'000'000;
	static constexpr uint64 kSimTicksPerSecond = 60;

	UPROPERTY(EditAnywhere, Category = "SFC|Sim", meta = (ClampMin = "0"))
	uint32 CaptureEveryNTicks = 6;

	FSfcOnCaptureTick OnCaptureTick;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	uint64 GetSimTickIndex() const { return SimTickIndex; }

	const TArray<FSfcCaptureStamp>& GetCaptureHistory() const { return CaptureHistory; }

	void ResetSimulationClock();

	void RegisterSpawner(ASeededPropSpawner* Spawner);
	void UnregisterSpawner(ASeededPropSpawner* Spawner);

	void AdvanceRealTimeMicroseconds(uint64 DeltaMicroseconds);

private:
	void ConsumeSimTicks(uint64 TickCount);
	void ConfigureCaptureTrigger();

	std::optional<sfc::FixedStepClock> SimClock;
	uint64 SimTickIndex = 0;

	sfc::CaptureTrigger CaptureTrigger;
	TArray<FSfcCaptureStamp> CaptureHistory;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASeededPropSpawner>> RegisteredSpawners;

	double TickRemainderMicroseconds = 0.0;
};
