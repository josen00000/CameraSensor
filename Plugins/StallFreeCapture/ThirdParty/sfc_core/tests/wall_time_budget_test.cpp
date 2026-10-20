#include "sfc/wall_time_budget.h"

#include <gtest/gtest.h>

namespace {

uint64_t SumSchedule(const std::vector<uint64_t>& schedule) {
  uint64_t total = 0;
  for (uint64_t delta : schedule) {
    total += delta;
  }
  return total;
}

}  // namespace

TEST(WallTimeBudgetTest, AlternatingScheduleSumsToBudget) {
  const auto schedule = sfc::BuildAlternatingWallSchedule(sfc::kSixtySecondWallBudgetUs);
  EXPECT_EQ(SumSchedule(schedule), sfc::kSixtySecondWallBudgetUs);
}

TEST(WallTimeBudgetTest, HitchScheduleSumsToBudgetAndProduces3600Ticks) {
  const auto base = sfc::BuildAlternatingWallSchedule(sfc::kSixtySecondWallBudgetUs);
  const auto hitch = sfc::BuildHitchWallSchedule(base);
  EXPECT_EQ(SumSchedule(hitch), sfc::kSixtySecondWallBudgetUs);

  auto clock = sfc::FixedStepClock::Create(1'000'000, 60);
  ASSERT_TRUE(clock.has_value());
  const sfc::WallBudgetRunResult result =
      sfc::RunWallSchedule(*clock, hitch, sfc::WallBudgetRunCallbacks{});
  EXPECT_EQ(result.ticks, 3600u);
  EXPECT_EQ(result.microseconds_fed, sfc::kSixtySecondWallBudgetUs);
}

TEST(WallTimeBudgetTest, NormalAndHitchSchedulesMatchTickCount) {
  const auto base = sfc::BuildAlternatingWallSchedule(sfc::kSixtySecondWallBudgetUs);
  const auto hitch = sfc::BuildHitchWallSchedule(base);

  auto clock_a = sfc::FixedStepClock::Create(1'000'000, 60);
  auto clock_b = sfc::FixedStepClock::Create(1'000'000, 60);
  ASSERT_TRUE(clock_a.has_value());
  ASSERT_TRUE(clock_b.has_value());

  const auto result_a = sfc::RunWallSchedule(*clock_a, base, sfc::WallBudgetRunCallbacks{});
  const auto result_b = sfc::RunWallSchedule(*clock_b, hitch, sfc::WallBudgetRunCallbacks{});
  EXPECT_EQ(result_a.ticks, 3600u);
  EXPECT_EQ(result_b.ticks, 3600u);
}

TEST(WallTimeBudgetTest, Fps30And120Produce3600Ticks) {
  const auto schedule_30 = sfc::BuildFpsWallSchedule(sfc::kSixtySecondWallBudgetUs, 30);
  const auto schedule_120 = sfc::BuildFpsWallSchedule(sfc::kSixtySecondWallBudgetUs, 120);
  EXPECT_EQ(SumSchedule(schedule_30), sfc::kSixtySecondWallBudgetUs);
  EXPECT_EQ(SumSchedule(schedule_120), sfc::kSixtySecondWallBudgetUs);

  auto clock_30 = sfc::FixedStepClock::Create(1'000'000, 60);
  auto clock_120 = sfc::FixedStepClock::Create(1'000'000, 60);
  ASSERT_TRUE(clock_30.has_value());
  ASSERT_TRUE(clock_120.has_value());

  const auto result_30 = sfc::RunWallSchedule(*clock_30, schedule_30, sfc::WallBudgetRunCallbacks{});
  const auto result_120 = sfc::RunWallSchedule(*clock_120, schedule_120, sfc::WallBudgetRunCallbacks{});
  EXPECT_EQ(result_30.ticks, 3600u);
  EXPECT_EQ(result_120.ticks, 3600u);
}
