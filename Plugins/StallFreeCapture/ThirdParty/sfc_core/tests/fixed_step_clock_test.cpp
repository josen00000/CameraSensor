#include "sfc/fixed_step_clock.h"

#include <gtest/gtest.h>

TEST(FixedStepClockTest, Advance205msAt60Hz) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  constexpr uint64_t kDeltaUs = 205000;
  auto result = clock.Advance(kDeltaUs);

  EXPECT_EQ(result.ticks_ready, 12);
  EXPECT_GT(result.leftover_microseconds, 0);
  EXPECT_LT(result.leftover_microseconds, kStepUs);
  EXPECT_EQ(result.leftover_microseconds, kDeltaUs - (12 * kStepUs));
}

TEST(FixedStepClockTest, HitchCarriesLeftover) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  auto result1 = clock.Advance(200000);
  uint64_t leftover1 = result1.leftover_microseconds;

  auto result2 = clock.Advance(5000);
  uint64_t total_available = leftover1 + 5000;
  int expected_ticks = static_cast<int>(total_available / kStepUs);
  uint64_t expected_leftover = total_available % kStepUs;

  EXPECT_EQ(result2.ticks_ready, expected_ticks);
  EXPECT_EQ(result2.leftover_microseconds, expected_leftover);
}

TEST(FixedStepClockTest, TinyFramesAccumulate) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  for (int i = 0; i < 10; ++i) {
    auto result = clock.Advance(1000);
    if (i < 10) {
      EXPECT_EQ(result.ticks_ready, 0);
    }
  }

  auto result = clock.Advance(7000);
  EXPECT_EQ(result.ticks_ready, 1);
}

TEST(FixedStepClockTest, ExactStepProducesOneTick) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  auto result = clock.Advance(kStepUs);
  EXPECT_EQ(result.ticks_ready, 1);
  EXPECT_EQ(result.leftover_microseconds, 0);
}

TEST(FixedStepClockTest, ThreeThousandSixHundredTicksFromExactTime) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  int total_ticks = 0;
  for (int i = 0; i < 3600; ++i) {
    auto result = clock.Advance(kStepUs);
    total_ticks += result.ticks_ready;
  }

  EXPECT_EQ(total_ticks, 3600);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 0);
}

TEST(FixedStepClockTest, ZeroDeltaProducesZeroTicks) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  auto result = clock.Advance(0);
  EXPECT_EQ(result.ticks_ready, 0);
  EXPECT_EQ(result.leftover_microseconds, 0);
}

TEST(FixedStepClockTest, GettersReturnCorrectValues) {
  constexpr uint64_t kStepUs = 16667;
  sfc::FixedStepClock clock(kStepUs);

  EXPECT_EQ(clock.GetStepMicroseconds(), kStepUs);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 0);

  clock.Advance(10000);
  EXPECT_EQ(clock.GetStepMicroseconds(), kStepUs);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 10000);
}
