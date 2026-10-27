#include "sfc/prop_sim.h"

#include <algorithm>
#include <cmath>
#include "sfc/pcg32.h"
#include "sfc/xxhash_wrapper.h"

namespace sfc {
namespace {

int32_t LerpMilli(int32_t min_milli, int32_t max_milli, uint32_t draw) {
  const uint32_t unit = draw >> 8;
  const int64_t span = static_cast<int64_t>(max_milli) - static_cast<int64_t>(min_milli);
  const int64_t offset = (span * static_cast<int64_t>(unit)) >> 24;
  return static_cast<int32_t>(static_cast<int64_t>(min_milli) + offset);
}

void AppendLE32(std::vector<uint8_t>& buffer, uint32_t value) {
  buffer.push_back(static_cast<uint8_t>(value & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
  buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
}

void AppendLE32(std::vector<uint8_t>& buffer, int32_t value) {
  AppendLE32(buffer, static_cast<uint32_t>(value));
}

void AppendPropRecord(std::vector<uint8_t>& buffer, const PropSimState& state) {
  AppendLE32(buffer, state.id);
  buffer.push_back(state.prop_class);
  AppendLE32(buffer, state.pos_milli[0]);
  AppendLE32(buffer, state.pos_milli[1]);
  AppendLE32(buffer, state.pos_milli[2]);
  AppendLE32(buffer, state.vel_milli_per_tick[0]);
  AppendLE32(buffer, state.vel_milli_per_tick[1]);
  AppendLE32(buffer, state.vel_milli_per_tick[2]);
}

}  // namespace

int32_t PositionToMilli(float value) {
  return static_cast<int32_t>(std::llround(static_cast<double>(value) * 1000.0));
}

float MilliToPosition(int32_t milli) {
  return static_cast<float>(milli) * 0.001f;
}

ArenaMilli ArenaToMilli(float half_x, float half_y, float half_z) {
  return ArenaMilli{PositionToMilli(half_x), PositionToMilli(half_y), PositionToMilli(half_z)};
}

void InitSimVelocities(uint64_t seed, PropSimState* states, int32_t count) {
  if (count <= 0 || states == nullptr) {
    return;
  }

  Pcg32 rng(seed ^ kPropSimVelocitySeedSalt);
  constexpr int32_t kMaxVelMilliPerTick = 8'000;

  for (int32_t index = 0; index < count; ++index) {
    PropSimState& state = states[index];
    for (int axis = 0; axis < 3; ++axis) {
      const uint32_t draw = rng.Next();
      state.vel_milli_per_tick[axis] =
          LerpMilli(-kMaxVelMilliPerTick, kMaxVelMilliPerTick, draw);
    }
  }
}

bool ClampPropInsideArena(PropSimState& state, const ArenaMilli& arena) {
  bool clamped = false;
  const int32_t limits[3] = {arena.half_x, arena.half_y, arena.half_z};
  for (int axis = 0; axis < 3; ++axis) {
    const int32_t min_p = -limits[axis];
    const int32_t max_p = limits[axis];
    if (state.pos_milli[axis] > max_p) {
      state.pos_milli[axis] = max_p;
      clamped = true;
    } else if (state.pos_milli[axis] < min_p) {
      state.pos_milli[axis] = min_p;
      clamped = true;
    }
  }
  return clamped;
}

bool StepPropSim(PropSimState& state, const ArenaMilli& arena) {
  bool hard_clamped = false;
  const int32_t limits[3] = {arena.half_x, arena.half_y, arena.half_z};
  for (int axis = 0; axis < 3; ++axis) {
    const int32_t min_p = -limits[axis];
    const int32_t max_p = limits[axis];

    int64_t position = static_cast<int64_t>(state.pos_milli[axis]) +
                       static_cast<int64_t>(state.vel_milli_per_tick[axis]);

    if (position > max_p) {
      position = static_cast<int64_t>(max_p) - (position - static_cast<int64_t>(max_p));
      state.vel_milli_per_tick[axis] = -state.vel_milli_per_tick[axis];
    } else if (position < min_p) {
      position = static_cast<int64_t>(min_p) + (static_cast<int64_t>(min_p) - position);
      state.vel_milli_per_tick[axis] = -state.vel_milli_per_tick[axis];
    }

    if (position > max_p) {
      position = max_p;
      state.vel_milli_per_tick[axis] = 0;
      hard_clamped = true;
    } else if (position < min_p) {
      position = min_p;
      state.vel_milli_per_tick[axis] = 0;
      hard_clamped = true;
    }

    state.pos_milli[axis] = static_cast<int32_t>(position);
  }
  return hard_clamped;
}

void StepAllPropSim(PropSimState* states, int32_t count, const ArenaMilli& arena) {
  if (states == nullptr || count <= 0) {
    return;
  }
  for (int32_t index = 0; index < count; ++index) {
    StepPropSim(states[index], arena);
  }
}

uint64_t HashPropSimStates(const PropSimState* states, int32_t count) {
  if (count <= 0 || states == nullptr) {
    return XxHash64(nullptr, 0, 0);
  }

  std::vector<const PropSimState*> sorted;
  sorted.reserve(static_cast<size_t>(count));
  for (int32_t index = 0; index < count; ++index) {
    sorted.push_back(&states[index]);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const PropSimState* left, const PropSimState* right) {
              return left->id < right->id;
            });

  std::vector<uint8_t> buffer;
  buffer.reserve(static_cast<size_t>(count) * 29u);
  for (const PropSimState* state : sorted) {
    AppendPropRecord(buffer, *state);
  }

  return XxHash64(buffer.data(), buffer.size(), 0);
}

}  // namespace sfc
