#include "sfc/fixed_step_clock.h"

namespace sfc {

FixedStepClock::FixedStepClock(uint64_t period_microseconds, uint64_t ticks_per_period)
    : period_us_(period_microseconds),
      ticks_per_period_(ticks_per_period),
      owed_scaled_(0) {}

std::optional<FixedStepClock> FixedStepClock::Create(uint64_t period_microseconds,
                                                     uint64_t ticks_per_period) {
  if (period_microseconds == 0 || ticks_per_period == 0) {
    return std::nullopt;
  }
  return FixedStepClock(period_microseconds, ticks_per_period);
}

FixedStepClock::AdvanceResult FixedStepClock::Advance(uint64_t delta_microseconds) {
  const uint64_t total_scaled = owed_scaled_ + delta_microseconds * ticks_per_period_;
  const uint64_t ticks = total_scaled / period_us_;
  owed_scaled_ = total_scaled % period_us_;

  return AdvanceResult{ticks, owed_scaled_ / ticks_per_period_};
}

uint64_t FixedStepClock::GetPeriodMicroseconds() const { return period_us_; }

uint64_t FixedStepClock::GetTicksPerPeriod() const { return ticks_per_period_; }

uint64_t FixedStepClock::GetLeftoverMicroseconds() const {
  return owed_scaled_ / ticks_per_period_;
}

}  // namespace sfc
