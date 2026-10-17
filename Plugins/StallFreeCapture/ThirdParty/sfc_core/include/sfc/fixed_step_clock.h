#pragma once

#include <cstdint>
#include <optional>

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
//
// Use Create() to construct; invalid parameters return std::nullopt (no exceptions).
class FixedStepClock {
 public:
  static std::optional<FixedStepClock> Create(uint64_t period_microseconds,
                                              uint64_t ticks_per_period);

  struct AdvanceResult {
    uint64_t ticks_ready;
    uint64_t leftover_microseconds;
  };

  AdvanceResult Advance(uint64_t delta_microseconds);

  uint64_t GetPeriodMicroseconds() const;
  uint64_t GetTicksPerPeriod() const;
  uint64_t GetLeftoverMicroseconds() const;

 private:
  FixedStepClock(uint64_t period_microseconds, uint64_t ticks_per_period);

  uint64_t period_us_;
  uint64_t ticks_per_period_;
  uint64_t owed_scaled_;
};

}  // namespace sfc
