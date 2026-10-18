#include "sfc/capture_trigger.h"

namespace sfc {

uint32_t SanitizeCaptureEveryNTicks(uint32_t requested, bool& out_was_clamped) {
  out_was_clamped = false;
  if (requested == 0) {
    out_was_clamped = true;
    return 1;
  }
  return requested;
}

void CaptureTrigger::Reset(uint32_t every_n_ticks) {
  bool clamped = false;
  every_n_ticks_ = SanitizeCaptureEveryNTicks(every_n_ticks, clamped);
  (void)clamped;
  next_seq_ = 0;
}

std::optional<CaptureStamp> CaptureTrigger::OnSimTick(uint64_t tick_index) {
  if (every_n_ticks_ == 0 || tick_index == 0 || (tick_index % every_n_ticks_) != 0) {
    return std::nullopt;
  }
  CaptureStamp stamp;
  stamp.seq = next_seq_++;
  stamp.tick_index = tick_index;
  return stamp;
}

}  // namespace sfc
