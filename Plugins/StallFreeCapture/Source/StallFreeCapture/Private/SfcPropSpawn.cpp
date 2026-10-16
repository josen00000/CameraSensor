// Copyright Epic Games, Inc. All Rights Reserved.

#include "SfcPropSpawn.h"

#include "sfc/pcg32.h"
#include "sfc/xxhash_wrapper.h"

namespace
{
	// Hash buffer is built with explicit little-endian field writes (see Append* below).
	static_assert(PLATFORM_LITTLE_ENDIAN, "FSfcPropSpawn hash layout assumes little-endian host");

	float UnitIntervalFromDraw(uint32 Draw)
	{
		return static_cast<float>(Draw >> 8) * 0x1p-24f;
	}

	float Lerp(float Min, float Max, float T)
	{
		return Min + (Max - Min) * T;
	}

	void AppendLE8(TArray<uint8>& Buffer, uint8 Value)
	{
		Buffer.Add(Value);
	}

	void AppendLE32(TArray<uint8>& Buffer, uint32 Value)
	{
		Buffer.Add(static_cast<uint8>(Value & 0xFFu));
		Buffer.Add(static_cast<uint8>((Value >> 8) & 0xFFu));
		Buffer.Add(static_cast<uint8>((Value >> 16) & 0xFFu));
		Buffer.Add(static_cast<uint8>((Value >> 24) & 0xFFu));
	}

	void AppendLE32(TArray<uint8>& Buffer, int32 Value)
	{
		AppendLE32(Buffer, static_cast<uint32>(Value));
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
		State.Id = Index + 1;
		State.RawDraws.Reset();

		const auto RecordDraw = [&Rng, &State]() -> uint32
		{
			const uint32 Draw = Rng.Next();
			State.RawDraws.Add(Draw);
			return Draw;
		};

		const uint32 ClassDraw = RecordDraw();
		State.PropClass = static_cast<ESfcPropClass>(ClassDraw % 3);

		const float PosX = Lerp(-ArenaHalfExtents.X, ArenaHalfExtents.X, UnitIntervalFromDraw(RecordDraw()));
		const float PosY = Lerp(-ArenaHalfExtents.Y, ArenaHalfExtents.Y, UnitIntervalFromDraw(RecordDraw()));
		const float PosZ = Lerp(-ArenaHalfExtents.Z, ArenaHalfExtents.Z, UnitIntervalFromDraw(RecordDraw()));
		State.Position = FVector(PosX, PosY, PosZ);

		const float Pitch = Lerp(0.0f, 360.0f, UnitIntervalFromDraw(RecordDraw()));
		const float Yaw = Lerp(0.0f, 360.0f, UnitIntervalFromDraw(RecordDraw()));
		const float Roll = Lerp(0.0f, 360.0f, UnitIntervalFromDraw(RecordDraw()));
		State.Rotation = FRotator(Pitch, Yaw, Roll);

		const float ScaleX = Lerp(0.5f, 2.0f, UnitIntervalFromDraw(RecordDraw()));
		const float ScaleY = Lerp(0.5f, 2.0f, UnitIntervalFromDraw(RecordDraw()));
		const float ScaleZ = Lerp(0.5f, 2.0f, UnitIntervalFromDraw(RecordDraw()));
		State.Scale = FVector(ScaleX, ScaleY, ScaleZ);

		OutStates.Add(State);
	}
}

uint64 FSfcPropSpawn::HashInitialState(const TArray<FSfcPropState>& States)
{
	TArray<uint8> Buffer;
	Buffer.Reserve(States.Num() * (sizeof(int32) + sizeof(uint8) + 10 * sizeof(uint32)));
	for (const FSfcPropState& State : States)
	{
		AppendLE32(Buffer, State.Id);
		AppendLE8(Buffer, static_cast<uint8>(State.PropClass));
		for (const uint32 Draw : State.RawDraws)
		{
			AppendLE32(Buffer, Draw);
		}
	}
	if (Buffer.IsEmpty())
	{
		return sfc::XxHash64(nullptr, 0, 0);
	}
	return sfc::XxHash64(Buffer.GetData(), static_cast<size_t>(Buffer.Num()), 0);
}
