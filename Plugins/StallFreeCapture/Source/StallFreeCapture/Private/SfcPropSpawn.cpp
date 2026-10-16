// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcPropSpawn.h"

#include "sfc/pcg32.h"
#include "sfc/xxhash_wrapper.h"

namespace
{
	float UnitInterval(uint32_t Value)
	{
		return static_cast<float>(Value) / static_cast<float>(UINT32_MAX);
	}

	float Lerp(float Min, float Max, float T)
	{
		return Min + (Max - Min) * T;
	}

#pragma pack(push, 1)
	struct FPackedPropState
	{
		uint16 Id;
		uint8 PropClass;
		uint8 Padding;
		float Position[3];
		float Rotation[3];
		float Scale[3];
	};
#pragma pack(pop)

	static_assert(sizeof(FPackedPropState) == 40, "Packed prop layout must be stable for hashing");

	FPackedPropState Pack(const FSfcPropState& State)
	{
		FPackedPropState Packed{};
		Packed.Id = State.Id;
		Packed.PropClass = static_cast<uint8>(State.PropClass);
		Packed.Padding = 0;
		Packed.Position[0] = State.Position.X;
		Packed.Position[1] = State.Position.Y;
		Packed.Position[2] = State.Position.Z;
		Packed.Rotation[0] = State.Rotation.Pitch;
		Packed.Rotation[1] = State.Rotation.Yaw;
		Packed.Rotation[2] = State.Rotation.Roll;
		Packed.Scale[0] = State.Scale.X;
		Packed.Scale[1] = State.Scale.Y;
		Packed.Scale[2] = State.Scale.Z;
		return Packed;
	}
}

int32 FSfcPropSpawn::SanitizePropCount(int32 RequestedCount, bool& OutWasClamped)
{
	OutWasClamped = false;
	if (RequestedCount > MaxPropCount)
	{
		OutWasClamped = true;
		return MaxPropCount;
	}
	if (RequestedCount < 0)
	{
		return 0;
	}
	return RequestedCount;
}

void FSfcPropSpawn::BuildStates(
	uint64 Seed,
	int32 PropCount,
	const FVector& ArenaHalfExtents,
	TArray<FSfcPropState>& OutStates)
{
	bool bClamped = false;
	const int32 SafeCount = SanitizePropCount(PropCount, bClamped);
	(void)bClamped;
	OutStates.Reset();
	OutStates.Reserve(SafeCount);

	sfc::Pcg32 Rng(Seed);
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		FSfcPropState State;
		State.Id = static_cast<uint16>(Index + 1);

		const uint32 ClassDraw = Rng.Next();
		State.PropClass = static_cast<ESfcPropClass>(ClassDraw % 3);

		const float PosX = Lerp(-ArenaHalfExtents.X, ArenaHalfExtents.X, UnitInterval(Rng.Next()));
		const float PosY = Lerp(-ArenaHalfExtents.Y, ArenaHalfExtents.Y, UnitInterval(Rng.Next()));
		const float PosZ = Lerp(-ArenaHalfExtents.Z, ArenaHalfExtents.Z, UnitInterval(Rng.Next()));
		State.Position = FVector(PosX, PosY, PosZ);

		const float Pitch = Lerp(0.0f, 360.0f, UnitInterval(Rng.Next()));
		const float Yaw = Lerp(0.0f, 360.0f, UnitInterval(Rng.Next()));
		const float Roll = Lerp(0.0f, 360.0f, UnitInterval(Rng.Next()));
		State.Rotation = FRotator(Pitch, Yaw, Roll);

		const float ScaleX = Lerp(0.5f, 2.0f, UnitInterval(Rng.Next()));
		const float ScaleY = Lerp(0.5f, 2.0f, UnitInterval(Rng.Next()));
		const float ScaleZ = Lerp(0.5f, 2.0f, UnitInterval(Rng.Next()));
		State.Scale = FVector(ScaleX, ScaleY, ScaleZ);

		OutStates.Add(State);
	}
}

uint64 FSfcPropSpawn::HashInitialState(const TArray<FSfcPropState>& States)
{
	TArray<uint8> Buffer;
	Buffer.Reserve(States.Num() * sizeof(FPackedPropState));
	for (const FSfcPropState& State : States)
	{
		const FPackedPropState Packed = Pack(State);
		const uint8* Bytes = reinterpret_cast<const uint8*>(&Packed);
		Buffer.Append(Bytes, sizeof(FPackedPropState));
	}
	if (Buffer.IsEmpty())
	{
		return sfc::XxHash64(nullptr, 0, 0);
	}
	return sfc::XxHash64(Buffer.GetData(), static_cast<size_t>(Buffer.Num()), 0);
}
