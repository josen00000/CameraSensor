#include "sfc/mutex_queue.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>

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

TYPED_TEST(QueueContractTest, CapacityAcceptsPushesThroughTheBound) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  EXPECT_EQ(queue.Capacity(), std::size_t{4});
  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);
}

TYPED_TEST(QueueContractTest, CapacityRejectsPushBeyondTheBound) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(5), sfc::PushStatus::Rejected);

  sfc::PopResult<int> first = queue.TryPop();
  ASSERT_EQ(first.status, sfc::PopStatus::Item);
  EXPECT_EQ(first.value, 1);

  sfc::PopResult<int> second = queue.TryPop();
  ASSERT_EQ(second.status, sfc::PopStatus::Item);
  EXPECT_EQ(second.value, 2);

  sfc::PopResult<int> third = queue.TryPop();
  ASSERT_EQ(third.status, sfc::PopStatus::Item);
  EXPECT_EQ(third.value, 3);

  sfc::PopResult<int> fourth = queue.TryPop();
  ASSERT_EQ(fourth.status, sfc::PopStatus::Item);
  EXPECT_EQ(fourth.value, 4);

  sfc::PopResult<int> empty = queue.TryPop();
  EXPECT_EQ(empty.status, sfc::PopStatus::Empty);
  EXPECT_FALSE(empty.value.has_value());
}

TYPED_TEST(QueueContractTest, CapacityReusesSlotsAfterPop) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);

  sfc::PopResult<int> first = queue.TryPop();
  ASSERT_EQ(first.status, sfc::PopStatus::Item);
  EXPECT_EQ(first.value, 1);

  sfc::PopResult<int> second = queue.TryPop();
  ASSERT_EQ(second.status, sfc::PopStatus::Item);
  EXPECT_EQ(second.value, 2);

  EXPECT_EQ(queue.Push(5), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(6), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(7), sfc::PushStatus::Rejected);

  sfc::PopResult<int> third = queue.TryPop();
  ASSERT_EQ(third.status, sfc::PopStatus::Item);
  EXPECT_EQ(third.value, 3);

  sfc::PopResult<int> fourth = queue.TryPop();
  ASSERT_EQ(fourth.status, sfc::PopStatus::Item);
  EXPECT_EQ(fourth.value, 4);

  sfc::PopResult<int> fifth = queue.TryPop();
  ASSERT_EQ(fifth.status, sfc::PopStatus::Item);
  EXPECT_EQ(fifth.value, 5);

  sfc::PopResult<int> sixth = queue.TryPop();
  ASSERT_EQ(sixth.status, sfc::PopStatus::Item);
  EXPECT_EQ(sixth.value, 6);

  sfc::PopResult<int> empty = queue.TryPop();
  EXPECT_EQ(empty.status, sfc::PopStatus::Empty);
  EXPECT_FALSE(empty.value.has_value());
}

TYPED_TEST(QueueContractTest, DropOldestEvictsOldestAndCountsDrops) {
  TypeParam queue(4, sfc::FullPolicy::DropOldest);

  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(5), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(6), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.DropCount(), std::uint64_t{2});

  sfc::PopResult<int> first = queue.TryPop();
  ASSERT_EQ(first.status, sfc::PopStatus::Item);
  EXPECT_EQ(first.value, 3);

  sfc::PopResult<int> second = queue.TryPop();
  ASSERT_EQ(second.status, sfc::PopStatus::Item);
  EXPECT_EQ(second.value, 4);

  sfc::PopResult<int> third = queue.TryPop();
  ASSERT_EQ(third.status, sfc::PopStatus::Item);
  EXPECT_EQ(third.value, 5);

  sfc::PopResult<int> fourth = queue.TryPop();
  ASSERT_EQ(fourth.status, sfc::PopStatus::Item);
  EXPECT_EQ(fourth.value, 6);

  sfc::PopResult<int> empty = queue.TryPop();
  EXPECT_EQ(empty.status, sfc::PopStatus::Empty);
  EXPECT_FALSE(empty.value.has_value());
}

TYPED_TEST(QueueContractTest, RejectRefusesPushAndKeepsDropCount) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  EXPECT_EQ(queue.Push(1), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(2), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(3), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(4), sfc::PushStatus::Pushed);
  EXPECT_EQ(queue.Push(5), sfc::PushStatus::Rejected);
  EXPECT_EQ(queue.DropCount(), std::uint64_t{0});

  sfc::PopResult<int> first = queue.TryPop();
  ASSERT_EQ(first.status, sfc::PopStatus::Item);
  EXPECT_EQ(first.value, 1);

  sfc::PopResult<int> second = queue.TryPop();
  ASSERT_EQ(second.status, sfc::PopStatus::Item);
  EXPECT_EQ(second.value, 2);

  sfc::PopResult<int> third = queue.TryPop();
  ASSERT_EQ(third.status, sfc::PopStatus::Item);
  EXPECT_EQ(third.value, 3);

  sfc::PopResult<int> fourth = queue.TryPop();
  ASSERT_EQ(fourth.status, sfc::PopStatus::Item);
  EXPECT_EQ(fourth.value, 4);

  sfc::PopResult<int> empty = queue.TryPop();
  EXPECT_EQ(empty.status, sfc::PopStatus::Empty);
  EXPECT_FALSE(empty.value.has_value());
}

TYPED_TEST(QueueContractTest, CloseWakesBlockedProducerWithinTwoSeconds) {
  TypeParam queue(1, sfc::FullPolicy::Block);
  ASSERT_EQ(queue.Push(1), sfc::PushStatus::Pushed);

  std::promise<sfc::PushStatus> promise;
  std::future<sfc::PushStatus> future = promise.get_future();
  std::atomic<bool> entered{false};

  std::thread producer([&queue, &promise, &entered] {
    entered.store(true, std::memory_order_release);
    promise.set_value(queue.Push(2));
  });

  struct CloseAndJoin {
    TypeParam* queue;
    std::thread* producer;
    ~CloseAndJoin() {
      queue->Close();
      if (producer->joinable()) {
        producer->join();
      }
    }
  } guard{&queue, &producer};

  while (!entered.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
  ASSERT_EQ(future.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

  queue.Close();
  ASSERT_EQ(future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(future.get(), sfc::PushStatus::Closed);

  sfc::PopResult<int> stored = queue.TryPop();
  ASSERT_EQ(stored.status, sfc::PopStatus::Item);
  EXPECT_EQ(stored.value, 1);

  sfc::PopResult<int> closed = queue.TryPop();
  EXPECT_EQ(closed.status, sfc::PopStatus::Closed);
  EXPECT_FALSE(closed.value.has_value());
}

template <typename Queue>
class MoveOnlyQueueContractTest : public ::testing::Test {};

using MoveOnlyQueueTypes =
    ::testing::Types<sfc::MutexBoundedQueue<std::unique_ptr<int>>>;
TYPED_TEST_SUITE(MoveOnlyQueueContractTest, MoveOnlyQueueTypes);

TYPED_TEST(MoveOnlyQueueContractTest, PushMovesPayloadAndPopYieldsIt) {
  TypeParam queue(4, sfc::FullPolicy::Reject);

  auto payload = std::make_unique<int>(42);
  EXPECT_EQ(queue.Push(std::move(payload)), sfc::PushStatus::Pushed);
  EXPECT_EQ(payload, nullptr);

  auto result = queue.TryPop();
  ASSERT_EQ(result.status, sfc::PopStatus::Item);
  ASSERT_TRUE(result.value.has_value());
  ASSERT_NE(*result.value, nullptr);
  EXPECT_EQ(**result.value, 42);
}

