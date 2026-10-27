#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace sfc {

struct ArenaMilli {
  int32_t half_x = 0;
  int32_t half_y = 0;
  int32_t half_z = 0;
};

struct PropSimState {
  int32_t id = 0;
  uint8_t prop_class = 0;
  int32_t pos_milli[3] = {0, 0, 0};
  int32_t vel_milli_per_tick[3] = {0, 0, 0};
};

constexpr uint64_t kPropSimVelocitySeedSalt = 0xC1A5EED7D4C0DEULL;

// Milli-units per Unreal unit (deterministic round).
int32_t PositionToMilli(float value);
float MilliToPosition(int32_t milli);

ArenaMilli ArenaToMilli(float half_x, float half_y, float half_z);

// Deterministic velocities from seed (does not alter spawn draws).
void InitSimVelocities(uint64_t seed, PropSimState* states, int32_t count);

// Clamp any position outside the arena; returns true if clamping occurred.
bool ClampPropInsideArena(PropSimState& state, const ArenaMilli& arena);

// One fixed 60 Hz tick of axis-aligned wall bounce. Returns true if a hard arena clamp was applied.
bool StepPropSim(PropSimState& state, const ArenaMilli& arena);

void StepAllPropSim(PropSimState* states, int32_t count, const ArenaMilli& arena);

// xxHash64 over props sorted by id; fixed 29-byte records, little-endian, no padding.
uint64_t HashPropSimStates(const PropSimState* states, int32_t count);

}  // namespace sfc
