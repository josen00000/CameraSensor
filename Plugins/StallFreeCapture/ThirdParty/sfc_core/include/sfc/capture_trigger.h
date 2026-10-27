#pragma once

#include <cstdint>
#include <optional>

namespace sfc {

struct CaptureStamp {
  uint64_t seq = 0;
  uint64_t tick_index = 0;
};

// CaptureEveryNTicks: 0 clamps to 1 (sets OutWasClamped).
uint32_t SanitizeCaptureEveryNTicks(uint32_t requested, bool& out_was_clamped);

class CaptureTrigger {
 public:
  void Reset(uint32_t every_n_ticks);

  // Call after each sim tick (1-based tick_index). Emits when tick_index % every_n == 0.
  std::optional<CaptureStamp> OnSimTick(uint64_t tick_index);

  uint32_t GetEveryNTicks() const { return every_n_ticks_; }

 private:
  uint32_t every_n_ticks_ = 1;
  uint64_t next_seq_ = 0;
};

}  // namespace sfc
