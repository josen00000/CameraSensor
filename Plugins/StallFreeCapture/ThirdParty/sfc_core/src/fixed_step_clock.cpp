#include "sfc/fixed_step_clock.h"

namespace sfc {

FixedStepClock::FixedStepClock(uint64_t step_microseconds)
    : step_us_(step_microseconds), leftover_us_(0) {}

FixedStepClock::AdvanceResult FixedStepClock::Advance(uint64_t delta_microseconds) {
  uint64_t total_us = leftover_us_ + delta_microseconds;
  int ticks = static_cast<int>(total_us / step_us_);
  leftover_us_ = total_us % step_us_;

  return AdvanceResult{ticks, leftover_us_};
}

uint64_t FixedStepClock::GetStepMicroseconds() const { return step_us_; }

uint64_t FixedStepClock::GetLeftoverMicroseconds() const { return leftover_us_; }

}  // namespace sfc
