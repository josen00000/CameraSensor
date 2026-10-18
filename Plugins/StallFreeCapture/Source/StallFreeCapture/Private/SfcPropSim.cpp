// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcPropSim.h"

sfc::ArenaMilli FSfcPropSim::ArenaToSimMilli(const FVector& ArenaHalfExtents)
{
	return sfc::ArenaToMilli(ArenaHalfExtents.X, ArenaHalfExtents.Y, ArenaHalfExtents.Z);
}

void FSfcPropSim::BuildSimStatesFromSpawn(
	uint64 Seed,
	const TArray<FSfcPropState>& PropStates,
	TArray<sfc::PropSimState>& OutSimStates)
{
	OutSimStates.Reset();
	OutSimStates.Reserve(PropStates.Num());

	for (const FSfcPropState& PropState : PropStates)
	{
		sfc::PropSimState SimState;
		SimState.id = PropState.Id;
		SimState.prop_class = static_cast<uint8_t>(PropState.PropClass);
		SimState.pos_milli[0] = sfc::PositionToMilli(PropState.Position.X);
		SimState.pos_milli[1] = sfc::PositionToMilli(PropState.Position.Y);
		SimState.pos_milli[2] = sfc::PositionToMilli(PropState.Position.Z);
		OutSimStates.Add(SimState);
	}

	if (OutSimStates.Num() > 0)
	{
		sfc::InitSimVelocities(Seed, OutSimStates.GetData(), OutSimStates.Num());
	}
}

void FSfcPropSim::SyncVisualStateFromSim(const sfc::PropSimState& SimState, FSfcPropState& PropState)
{
	PropState.Position.X = sfc::MilliToPosition(SimState.pos_milli[0]);
	PropState.Position.Y = sfc::MilliToPosition(SimState.pos_milli[1]);
	PropState.Position.Z = sfc::MilliToPosition(SimState.pos_milli[2]);
}

uint64 FSfcPropSim::HashSimStates(const TArray<sfc::PropSimState>& SimStates)
{
	if (SimStates.IsEmpty())
	{
		return sfc::HashPropSimStates(nullptr, 0);
	}
	return sfc::HashPropSimStates(SimStates.GetData(), SimStates.Num());
}

void FSfcPropSim::StepSimTick(
	TArray<sfc::PropSimState>& SimStates,
	const sfc::ArenaMilli& Arena,
	TArray<int32>* OutStepClampedPropIds)
{
	if (SimStates.IsEmpty())
	{
		return;
	}

	if (OutStepClampedPropIds != nullptr)
	{
		OutStepClampedPropIds->Reset();
	}

	for (sfc::PropSimState& SimState : SimStates)
	{
		if (sfc::StepPropSim(SimState, Arena))
		{
			if (OutStepClampedPropIds != nullptr)
			{
				OutStepClampedPropIds->Add(SimState.id);
			}
		}
	}
}
