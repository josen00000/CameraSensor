#include "sfc/fixed_step_clock.h"

#include <gtest/gtest.h>

namespace {

constexpr uint64_t kPeriodUs = 1'000'000;
constexpr uint64_t kTicksPerPeriod = 60;

sfc::FixedStepClock Make60HzClock() {
  return sfc::FixedStepClock(kPeriodUs, kTicksPerPeriod);
}

}  // namespace

TEST(FixedStepClockTest, RejectsZeroPeriodOrTicks) {
  EXPECT_THROW(sfc::FixedStepClock(0, 60), std::invalid_argument);
  EXPECT_THROW(sfc::FixedStepClock(1'000'000, 0), std::invalid_argument);
}

TEST(FixedStepClockTest, Advance205msAt60Hz) {
  sfc::FixedStepClock clock = Make60HzClock();

  constexpr uint64_t kDeltaUs = 205'000;
  auto result = clock.Advance(kDeltaUs);

  EXPECT_EQ(result.ticks_ready, 12u);
  EXPECT_EQ(result.leftover_microseconds, 5'000u);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 5'000u);
}

TEST(FixedStepClockTest, OneSecondProducesSixtyTicks) {
  sfc::FixedStepClock clock = Make60HzClock();

  auto result = clock.Advance(kPeriodUs);
  EXPECT_EQ(result.ticks_ready, 60u);
  EXPECT_EQ(result.leftover_microseconds, 0u);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 0u);
}

TEST(FixedStepClockTest, SixtySecondsProduces3600Ticks) {
  sfc::FixedStepClock clock = Make60HzClock();

  uint64_t total_ticks = 0;
  for (int i = 0; i < 60; ++i) {
    auto result = clock.Advance(kPeriodUs);
    total_ticks += result.ticks_ready;
  }

  EXPECT_EQ(total_ticks, 3600u);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 0u);
}

TEST(FixedStepClockTest, HitchCarriesLeftover) {
  sfc::FixedStepClock clock = Make60HzClock();

  auto result1 = clock.Advance(200'000);
  uint64_t leftover1 = result1.leftover_microseconds;

  auto result2 = clock.Advance(5'000);
  const uint64_t total_us = leftover1 + 5'000;
  const uint64_t expected_ticks = (total_us * kTicksPerPeriod) / kPeriodUs;
  const uint64_t expected_leftover_us =
      ((total_us * kTicksPerPeriod) % kPeriodUs) / kTicksPerPeriod;

  EXPECT_EQ(result2.ticks_ready, expected_ticks);
  EXPECT_EQ(result2.leftover_microseconds, expected_leftover_us);
}

TEST(FixedStepClockTest, TinyFramesAccumulate) {
  sfc::FixedStepClock clock = Make60HzClock();

  for (int i = 0; i < 10; ++i) {
    auto result = clock.Advance(1'000);
    EXPECT_EQ(result.ticks_ready, 0u);
  }

  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 10'000u);

  auto result = clock.Advance(7'000);
  EXPECT_EQ(result.ticks_ready, 1u);
}

TEST(FixedStepClockTest, ZeroDeltaProducesZeroTicks) {
  sfc::FixedStepClock clock = Make60HzClock();

  auto result = clock.Advance(0);
  EXPECT_EQ(result.ticks_ready, 0u);
  EXPECT_EQ(result.leftover_microseconds, 0u);
}

TEST(FixedStepClockTest, GettersReturnCorrectValues) {
  sfc::FixedStepClock clock = Make60HzClock();

  EXPECT_EQ(clock.GetPeriodMicroseconds(), kPeriodUs);
  EXPECT_EQ(clock.GetTicksPerPeriod(), kTicksPerPeriod);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 0u);

  clock.Advance(10'000);
  EXPECT_EQ(clock.GetLeftoverMicroseconds(), 10'000u);
}
