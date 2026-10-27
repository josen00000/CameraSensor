#pragma once

#include <cstddef>

#include <gtest/gtest.h>

namespace sfc::test {

// Base fixture for typed queue contract suites. Instantiate with
// TYPED_TEST_SUITE(YourSuite, ::testing::Types<...>) and TYPED_TEST(YourSuite, ...).
template <typename Queue>
class QueueTypedFixture : public ::testing::Test {
 protected:
  using QueueType = Queue;

  static constexpr std::size_t kDefaultCapacity = 4;
};

}  // namespace sfc::test
