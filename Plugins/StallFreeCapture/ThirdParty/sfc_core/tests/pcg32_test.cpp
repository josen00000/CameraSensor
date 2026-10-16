#include "sfc/pcg32.h"

#include <gtest/gtest.h>
#include <utility>

TEST(Pcg32Test, ReferenceVectorsSixDraws) {
  sfc::Pcg32 rng(42, 54);

  EXPECT_EQ(rng.Next(), 0xa15c02b7u);
  EXPECT_EQ(rng.Next(), 0x7b47f409u);
  EXPECT_EQ(rng.Next(), 0xba1d3330u);
  EXPECT_EQ(rng.Next(), 0x83d2f293u);
  EXPECT_EQ(rng.Next(), 0xbfa4784bu);
  EXPECT_EQ(rng.Next(), 0xcbed606eu);
}

TEST(Pcg32Test, DifferentSeedsProduceDifferentValues) {
  sfc::Pcg32 rng1(42, 54);
  sfc::Pcg32 rng2(43, 54);

  uint32_t val1 = rng1.Next();
  uint32_t val2 = rng2.Next();

  EXPECT_NE(val1, val2);
}

TEST(Pcg32Test, DifferentStreamsProduceDifferentValues) {
  sfc::Pcg32 rng1(42, 54);
  sfc::Pcg32 rng2(42, 55);

  uint32_t val1 = rng1.Next();
  uint32_t val2 = rng2.Next();

  EXPECT_NE(val1, val2);
}

TEST(Pcg32Test, ReseedProducesSameSequence) {
  sfc::Pcg32 rng(42, 54);
  uint32_t first1 = rng.Next();
  uint32_t second1 = rng.Next();

  rng.Seed(42, 54);
  uint32_t first2 = rng.Next();
  uint32_t second2 = rng.Next();

  EXPECT_EQ(first1, first2);
  EXPECT_EQ(second1, second2);
}

TEST(Pcg32Test, SnapshotAndCopyReplaySequence) {
  sfc::Pcg32 original(42, 54);
  (void)original.Next();
  (void)original.Next();

  const auto snapshot = original.GetState();
  const uint32_t next_from_original = original.Next();

  sfc::Pcg32 replay(0, 0);
  EXPECT_TRUE(replay.SetState(snapshot));
  EXPECT_EQ(replay.Next(), next_from_original);

  sfc::Pcg32 copied(original);
  const uint32_t following = original.Next();
  EXPECT_EQ(copied.Next(), following);
}

TEST(Pcg32Test, SetStateRejectsInvalidSnapshot) {
  sfc::Pcg32 reference(42, 54);
  (void)reference.Next();
  (void)reference.Next();
  const auto before = reference.GetState();
  const uint32_t third_draw = reference.Next();

  sfc::Pcg32 rng(42, 54);
  (void)rng.Next();
  (void)rng.Next();

  sfc::Pcg32::Snapshot bad = before;
  bad.multiplier += 1;
  EXPECT_FALSE(rng.SetState(bad));
  EXPECT_EQ(rng.GetState().multiplier, before.multiplier);
  EXPECT_EQ(rng.GetState().inc, before.inc);
  EXPECT_EQ(rng.GetState().state, before.state);
  EXPECT_EQ(rng.Next(), third_draw);
}

TEST(Pcg32Test, MoveLeavesSourceValid) {
  sfc::Pcg32 donor(42, 54);
  const uint32_t first = donor.Next();

  sfc::Pcg32 moved(std::move(donor));
  const uint32_t second = moved.Next();

  sfc::Pcg32 expected(42, 54);
  EXPECT_EQ(expected.Next(), first);
  EXPECT_EQ(expected.Next(), second);

  const uint32_t from_donor = donor.Next();
  sfc::Pcg32 fresh(0, 0);
  EXPECT_EQ(from_donor, fresh.Next());
}
