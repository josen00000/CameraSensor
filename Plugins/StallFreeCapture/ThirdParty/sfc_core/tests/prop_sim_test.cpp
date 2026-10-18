#include "sfc/prop_sim.h"

#include "sfc/fixed_step_clock.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

sfc::ArenaMilli DefaultArena() {
  return sfc::ArenaToMilli(1000.0f, 1000.0f, 200.0f);
}

std::vector<sfc::PropSimState> MakeTwoProps() {
  std::vector<sfc::PropSimState> states(2);
  states[0].id = 1;
  states[0].prop_class = 0;
  states[0].pos_milli[0] = 0;
  states[0].pos_milli[1] = 0;
  states[0].pos_milli[2] = 0;
  states[0].vel_milli_per_tick[0] = 5'000;
  states[0].vel_milli_per_tick[1] = 0;
  states[0].vel_milli_per_tick[2] = 0;

  states[1].id = 2;
  states[1].prop_class = 1;
  states[1].pos_milli[0] = -100'000;
  states[1].pos_milli[1] = 0;
  states[1].pos_milli[2] = 0;
  states[1].vel_milli_per_tick[0] = 1'000;
  states[1].vel_milli_per_tick[1] = 0;
  states[1].vel_milli_per_tick[2] = 0;
  return states;
}

}  // namespace

TEST(PropSimTest, BounceKeepsPropsInsideArena) {
  const sfc::ArenaMilli arena = DefaultArena();
  std::vector<sfc::PropSimState> states = MakeTwoProps();

  for (int tick = 0; tick < 3600; ++tick) {
    sfc::StepAllPropSim(states.data(), static_cast<int32_t>(states.size()), arena);
    for (const sfc::PropSimState& state : states) {
      EXPECT_LE(state.pos_milli[0], arena.half_x);
      EXPECT_GE(state.pos_milli[0], -arena.half_x);
      EXPECT_LE(state.pos_milli[1], arena.half_y);
      EXPECT_GE(state.pos_milli[1], -arena.half_y);
      EXPECT_LE(state.pos_milli[2], arena.half_z);
      EXPECT_GE(state.pos_milli[2], -arena.half_z);
    }
  }
}

TEST(PropSimTest, HashIsStableAcrossRuns) {
  const sfc::ArenaMilli arena = DefaultArena();
  std::vector<sfc::PropSimState> states = MakeTwoProps();
  sfc::InitSimVelocities(7, states.data(), static_cast<int32_t>(states.size()));

  for (int tick = 0; tick < 100; ++tick) {
    sfc::StepAllPropSim(states.data(), static_cast<int32_t>(states.size()), arena);
  }

  const uint64_t hash_a = sfc::HashPropSimStates(states.data(), static_cast<int32_t>(states.size()));

  std::vector<sfc::PropSimState> states_b = MakeTwoProps();
  sfc::InitSimVelocities(7, states_b.data(), static_cast<int32_t>(states_b.size()));
  for (int tick = 0; tick < 100; ++tick) {
    sfc::StepAllPropSim(states_b.data(), static_cast<int32_t>(states_b.size()), arena);
  }
  const uint64_t hash_b = sfc::HashPropSimStates(states_b.data(), static_cast<int32_t>(states_b.size()));

  EXPECT_EQ(hash_a, hash_b);
}

TEST(PropSimTest, ClampDetectsOutsideArena) {
  const sfc::ArenaMilli arena = DefaultArena();
  sfc::PropSimState state{};
  state.id = 1;
  state.pos_milli[0] = arena.half_x + 10'000;
  EXPECT_TRUE(sfc::ClampPropInsideArena(state, arena));
  EXPECT_EQ(state.pos_milli[0], arena.half_x);
}

TEST(PropSimTest, HitchDoesNotChangeTick3600Hash) {
  const sfc::ArenaMilli arena = DefaultArena();
  constexpr int32_t kPropCount = 8;

  auto run_direct = [&arena]() {
    std::vector<sfc::PropSimState> states(static_cast<size_t>(kPropCount));
    for (int32_t index = 0; index < kPropCount; ++index) {
      states[static_cast<size_t>(index)].id = index + 1;
      states[static_cast<size_t>(index)].prop_class = static_cast<uint8_t>(index % 3);
      states[static_cast<size_t>(index)].pos_milli[0] = index * 10'000;
    }
    sfc::InitSimVelocities(7, states.data(), kPropCount);
    for (uint64_t tick = 1; tick <= 3600; ++tick) {
      sfc::StepAllPropSim(states.data(), kPropCount, arena);
    }
    return sfc::HashPropSimStates(states.data(), kPropCount);
  };

  auto run_with_hitch = [&arena]() {
    std::vector<sfc::PropSimState> states(static_cast<size_t>(kPropCount));
    for (int32_t index = 0; index < kPropCount; ++index) {
      states[static_cast<size_t>(index)].id = index + 1;
      states[static_cast<size_t>(index)].prop_class = static_cast<uint8_t>(index % 3);
      states[static_cast<size_t>(index)].pos_milli[0] = index * 10'000;
    }
    sfc::InitSimVelocities(7, states.data(), kPropCount);
    const sfc::ArenaMilli arena = DefaultArena();

    auto clock = sfc::FixedStepClock::Create(1'000'000, 60);
    EXPECT_TRUE(clock.has_value());
    uint64_t sim_ticks = 0;
    while (sim_ticks < 3600) {
      uint64_t delta_us = 16'667;
      if (sim_ticks == 999) {
        delta_us = 200'000;
      }
      const auto advance = clock->Advance(delta_us);
      for (uint64_t step = 0; step < advance.ticks_ready; ++step) {
        ++sim_ticks;
        sfc::StepAllPropSim(states.data(), kPropCount, arena);
        if (sim_ticks >= 3600) {
          break;
        }
      }
    }
    return sfc::HashPropSimStates(states.data(), kPropCount);
  };

  EXPECT_EQ(run_direct(), run_with_hitch());
}
