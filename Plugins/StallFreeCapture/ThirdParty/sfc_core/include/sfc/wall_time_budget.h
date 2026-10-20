#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "sfc/fixed_step_clock.h"

namespace sfc {

constexpr uint64_t kSixtySecondWallBudgetUs = 60'000'000;
constexpr uint64_t kHitchFrameMicroseconds = 200'000;
constexpr uint64_t kAlternatingFrameEvenUs = 16'666;
constexpr uint64_t kAlternatingFrameOddUs = 16'667;

struct WallBudgetRunResult {
  uint64_t ticks = 0;
  uint64_t microseconds_fed = 0;
};

// Frame deltas that sum to budget_us (alternating 16666/16667, last frame trimmed).
std::vector<uint64_t> BuildAlternatingWallSchedule(uint64_t budget_us);

// Same total microseconds; one frame near tick 1000 becomes kHitchFrameMicroseconds.
std::vector<uint64_t> BuildHitchWallSchedule(const std::vector<uint64_t>& base_schedule);

// Constant frame duration schedule (fps frames per second over budget_us).
std::vector<uint64_t> BuildFpsWallSchedule(uint64_t budget_us, uint64_t frames_per_second);

struct WallBudgetRunCallbacks {
  std::function<void(uint64_t /*tick_index_1_based*/)> on_tick;
};

WallBudgetRunResult RunWallSchedule(FixedStepClock& clock,
                                    const std::vector<uint64_t>& frame_deltas_us,
                                    const WallBudgetRunCallbacks& callbacks);

}  // namespace sfc
