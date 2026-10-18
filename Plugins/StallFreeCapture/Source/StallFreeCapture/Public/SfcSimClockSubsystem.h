// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
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

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }

	uint64 GetSimTickIndex() const { return SimTickIndex; }

	void ResetSimulationClock();

	void RegisterSpawner(ASeededPropSpawner* Spawner);
	void UnregisterSpawner(ASeededPropSpawner* Spawner);

	void AdvanceRealTimeMicroseconds(uint64 DeltaMicroseconds);

private:
	void ConsumeSimTicks(uint64 TickCount);

	std::optional<sfc::FixedStepClock> SimClock;
	uint64 SimTickIndex = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASeededPropSpawner>> RegisteredSpawners;
};
