#pragma once

#include <cstdint>
#include <stdexcept>

namespace sfc {

// Fixed-rate simulation clock with an exact rational timestep.
//
// Timestep: one tick every (period_microseconds / ticks_per_period) real µs.
// For 60 Hz use period_microseconds = 1'000'000 and ticks_per_period = 60.
//
// Time owed is accumulated as (microseconds * ticks_per_period) so ticks are
// counted every period_microseconds in that scaled space (no 16667 µs drift).
//
// Advance() takes non-negative delta_microseconds; callers must clamp negative
// frame deltas before calling.
class FixedStepClock {
 public:
  FixedStepClock(uint64_t period_microseconds, uint64_t ticks_per_period);

  struct AdvanceResult {
    uint64_t ticks_ready;
    uint64_t leftover_microseconds;
  };

  AdvanceResult Advance(uint64_t delta_microseconds);

  uint64_t GetPeriodMicroseconds() const;
  uint64_t GetTicksPerPeriod() const;
  uint64_t GetLeftoverMicroseconds() const;

 private:
  uint64_t period_us_;
  uint64_t ticks_per_period_;
  uint64_t owed_scaled_;
};

inline FixedStepClock::FixedStepClock(uint64_t period_microseconds,
                                      uint64_t ticks_per_period)
    : period_us_(period_microseconds),
      ticks_per_period_(ticks_per_period),
      owed_scaled_(0) {
  if (period_microseconds == 0 || ticks_per_period == 0) {
    throw std::invalid_argument(
        "FixedStepClock period and ticks_per_period must be non-zero");
  }
}

}  // namespace sfc
