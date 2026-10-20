#include "sfc/capture_trigger.h"

#include <gtest/gtest.h>

#include <vector>

TEST(CaptureTriggerTest, SixTicksOver3600) {
  sfc::CaptureTrigger trigger;
  trigger.Reset(6);

  std::vector<sfc::CaptureStamp> stamps;
  for (uint64_t tick = 1; tick <= 3600; ++tick) {
    const auto stamp = trigger.OnSimTick(tick);
    if (stamp.has_value()) {
      stamps.push_back(*stamp);
    }
  }

  EXPECT_EQ(stamps.size(), 600u);
  EXPECT_EQ(stamps.front().seq, 0u);
  EXPECT_EQ(stamps.front().tick_index, 6u);
  EXPECT_EQ(stamps.back().seq, 599u);
  EXPECT_EQ(stamps.back().tick_index, 3600u);
}

TEST(CaptureTriggerTest, EveryZeroClampsToOne) {
  bool clamped = false;
  EXPECT_EQ(sfc::SanitizeCaptureEveryNTicks(0, clamped), 1u);
  EXPECT_TRUE(clamped);
}
