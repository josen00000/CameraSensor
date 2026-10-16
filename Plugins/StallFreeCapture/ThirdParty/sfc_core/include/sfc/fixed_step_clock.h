#pragma once

#include <cstdint>

namespace sfc {

class FixedStepClock {
 public:
  explicit FixedStepClock(uint64_t step_microseconds);

  struct AdvanceResult {
    int ticks_ready;
    uint64_t leftover_microseconds;
  };

  AdvanceResult Advance(uint64_t delta_microseconds);

  uint64_t GetStepMicroseconds() const;
  uint64_t GetLeftoverMicroseconds() const;

 private:
  uint64_t step_us_;
  uint64_t leftover_us_;
};

}  // namespace sfc
