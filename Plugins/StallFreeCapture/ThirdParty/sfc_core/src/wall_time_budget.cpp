#include "sfc/wall_time_budget.h"

#include <algorithm>

namespace sfc {
namespace {

uint64_t AlternatingDeltaForIndex(size_t frame_index) {
  return (frame_index % 2 == 0) ? kAlternatingFrameEvenUs : kAlternatingFrameOddUs;
}

size_t FindHitchFrameIndex(const std::vector<uint64_t>& schedule) {
  auto clock = FixedStepClock::Create(1'000'000, 60);
  if (!clock.has_value()) {
    return 0;
  }
  uint64_t ticks = 0;
  for (size_t frame_index = 0; frame_index < schedule.size(); ++frame_index) {
    if (ticks == 999) {
      return frame_index;
    }
    ticks += clock->Advance(schedule[frame_index]).ticks_ready;
  }
  return schedule.size();
}

}  // namespace

std::vector<uint64_t> BuildAlternatingWallSchedule(uint64_t budget_us) {
  std::vector<uint64_t> frames;
  uint64_t remaining = budget_us;
  size_t frame_index = 0;
  while (remaining > 0) {
    uint64_t delta = AlternatingDeltaForIndex(frame_index);
    if (delta > remaining) {
      delta = remaining;
    }
    frames.push_back(delta);
    remaining -= delta;
    ++frame_index;
  }
  return frames;
}

std::vector<uint64_t> BuildHitchWallSchedule(const std::vector<uint64_t>& base_schedule) {
  std::vector<uint64_t> hitch = base_schedule;
  if (hitch.empty()) {
    return hitch;
  }

  const size_t hitch_frame = FindHitchFrameIndex(base_schedule);
  if (hitch_frame >= hitch.size()) {
    return hitch;
  }

  const uint64_t replaced = hitch[hitch_frame];
  uint64_t extra = kHitchFrameMicroseconds - replaced;
  hitch[hitch_frame] = kHitchFrameMicroseconds;

  for (size_t index = hitch.size(); index-- > 0 && extra > 0;) {
    if (index == hitch_frame) {
      continue;
    }
    const uint64_t peel = std::min(hitch[index], extra);
    hitch[index] -= peel;
    extra -= peel;
  }

  while (!hitch.empty() && hitch.back() == 0) {
    hitch.pop_back();
  }

  return hitch;
}

std::vector<uint64_t> BuildFpsWallSchedule(uint64_t budget_us, uint64_t frames_per_second) {
  std::vector<uint64_t> frames;
  if (frames_per_second == 0 || budget_us == 0) {
    return frames;
  }

  const uint64_t frame_count = (budget_us * frames_per_second) / 1'000'000;
  if (frame_count == 0) {
    return frames;
  }

  uint64_t remaining = budget_us;
  const uint64_t base_delta = budget_us / frame_count;
  uint64_t remainder = budget_us % frame_count;

  for (uint64_t index = 0; index < frame_count; ++index) {
    uint64_t delta = base_delta;
    if (remainder > 0) {
      ++delta;
      --remainder;
    }
    if (delta > remaining) {
      delta = remaining;
    }
    frames.push_back(delta);
    remaining -= delta;
  }

  while (remaining > 0) {
    frames.push_back(remaining);
    remaining = 0;
  }

  return frames;
}

WallBudgetRunResult RunWallSchedule(FixedStepClock& clock,
                                    const std::vector<uint64_t>& frame_deltas_us,
                                    const WallBudgetRunCallbacks& callbacks) {
  WallBudgetRunResult result;
  for (uint64_t delta : frame_deltas_us) {
    result.microseconds_fed += delta;
    const FixedStepClock::AdvanceResult advance = clock.Advance(delta);
    for (uint64_t step = 0; step < advance.ticks_ready; ++step) {
      ++result.ticks;
      if (callbacks.on_tick) {
        callbacks.on_tick(result.ticks);
      }
    }
  }
  return result;
}

}  // namespace sfc
