#include "sfc/mutex_queue.h"

#include "support/typed_fixture.h"

#include <gtest/gtest.h>

namespace {

template <typename Queue>
class MutexQueueTypedSampleTest : public sfc::test::QueueTypedFixture<Queue> {};

using MutexQueueTypedSampleTypes = ::testing::Types<sfc::MutexBoundedQueue<int>>;
TYPED_TEST_SUITE(MutexQueueTypedSampleTest, MutexQueueTypedSampleTypes);

TYPED_TEST(MutexQueueTypedSampleTest, DefaultFixtureCapacityIsFour) {
  TypeParam queue(this->kDefaultCapacity, sfc::FullPolicy::Reject);
  EXPECT_EQ(queue.Capacity(), std::size_t{4});
}

TYPED_TEST(MutexQueueTypedSampleTest, PushPopRoundTrip) {
  TypeParam queue(this->kDefaultCapacity, sfc::FullPolicy::Reject);
  EXPECT_EQ(queue.Push(7), sfc::PushStatus::Pushed);

  sfc::PopResult<int> result = queue.TryPop();
  ASSERT_EQ(result.status, sfc::PopStatus::Item);
  EXPECT_EQ(result.value, 7);
}

TYPED_TEST(MutexQueueTypedSampleTest, RejectPolicyRejectsWhenFull) {
  TypeParam queue(this->kDefaultCapacity, sfc::FullPolicy::Reject);
  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(5), sfc::PushStatus::Rejected);
}

}  // namespace
