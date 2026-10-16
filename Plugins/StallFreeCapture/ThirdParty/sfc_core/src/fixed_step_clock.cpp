#include "sfc/fixed_step_clock.h"

namespace sfc {

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
