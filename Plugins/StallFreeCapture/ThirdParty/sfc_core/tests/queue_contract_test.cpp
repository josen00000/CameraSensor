#include "sfc/mutex_queue.h"

#include <gtest/gtest.h>

template <typename Queue>
class QueueContractTest : public ::testing::Test {};

using QueueTypes = ::testing::Types<sfc::MutexBoundedQueue<int>>;
TYPED_TEST_SUITE(QueueContractTest, QueueTypes);

TYPED_TEST(QueueContractTest, TryPopOnEmptyReturnsEmpty) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  sfc::PopResult<int> result = queue.TryPop();

  EXPECT_EQ(result.status, sfc::PopStatus::Empty);
  EXPECT_FALSE(result.value.has_value());
}

TYPED_TEST(QueueContractTest, FifoReturnsItemsInPushOrder) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);

  sfc::PopResult<int> first = queue.TryPop();
  ASSERT_EQ(first.status, sfc::PopStatus::Item);
  EXPECT_EQ(first.value, 1);

  sfc::PopResult<int> second = queue.TryPop();
  ASSERT_EQ(second.status, sfc::PopStatus::Item);
  EXPECT_EQ(second.value, 2);

  sfc::PopResult<int> third = queue.TryPop();
  ASSERT_EQ(third.status, sfc::PopStatus::Item);
  EXPECT_EQ(third.value, 3);
}

