#include "sfc/mutex_queue.h"
#include "sfc/mpsc_ring.h"

#include "support/hang_guard.h"
#include "support/thread_runner.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

template <typename Queue>
class QueueContractTest : public ::testing::Test {};

using QueueTypes = ::testing::Types<sfc::MutexBoundedQueue<int>,
                                     sfc::LockFreeMpscRing<int>>;
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
  if (std::is_same_v<TypeParam, sfc::LockFreeMpscRing<int>>) {
    GTEST_SKIP() << "DropOldest not implemented for LockFreeMpscRing (see P3-T28)";
  }

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
    ::testing::Types<sfc::MutexBoundedQueue<std::unique_ptr<int>>,
                     sfc::LockFreeMpscRing<std::unique_ptr<int>>>;
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

TEST(LockFreeMpscRingMultiProducerTest, PreservesPerProducerOrderWithFourProducers) {
  constexpr size_t kProducerCount = 4;
  constexpr size_t kItemsPerProducer = 100'000;
  constexpr size_t kTotalItems = kProducerCount * kItemsPerProducer;

  sfc::LockFreeMpscRing<std::pair<size_t, size_t>> queue(128, sfc::FullPolicy::Block);
  std::vector<std::vector<size_t>> received_per_producer(kProducerCount);

  sfc::test::RunWithHangGuardOrFail(
      [&](std::atomic<bool>& stop) {
        std::vector<std::jthread> producers;
        producers.reserve(kProducerCount);

        for (size_t producer_id = 0; producer_id < kProducerCount; ++producer_id) {
          producers.emplace_back([&, producer_id] {
            for (size_t i = 0; i < kItemsPerProducer && !stop.load(std::memory_order_acquire);
                 ++i) {
              sfc::PushStatus status = queue.Push(std::make_pair(producer_id, i));
              if (status != sfc::PushStatus::Pushed) {
                ADD_FAILURE() << "producer " << producer_id << " push failed at item " << i
                             << " with status " << static_cast<int>(status);
                queue.Close();
                return;
              }
            }
          });
        }

        for (auto& vec : received_per_producer) {
          vec.reserve(kItemsPerProducer);
        }

        size_t total_received = 0;
        while (total_received < kTotalItems && !stop.load(std::memory_order_acquire)) {
          sfc::PopResult<std::pair<size_t, size_t>> result = queue.TryPop();
          if (result.status == sfc::PopStatus::Item) {
            if (!result.value.has_value()) {
              ADD_FAILURE() << "PopStatus::Item but no value";
              queue.Close();
              break;
            }
            auto [producer_id, sequence] = *result.value;
            if (producer_id >= kProducerCount) {
              ADD_FAILURE() << "invalid producer_id: " << producer_id;
              queue.Close();
              break;
            }
            received_per_producer[producer_id].push_back(sequence);
            ++total_received;
          } else if (result.status == sfc::PopStatus::Empty) {
            std::this_thread::yield();
          } else {
            ADD_FAILURE() << "unexpected PopStatus: " << static_cast<int>(result.status);
            queue.Close();
            break;
          }
        }

        producers.clear();

        ASSERT_EQ(total_received, kTotalItems);

        for (size_t producer_id = 0; producer_id < kProducerCount; ++producer_id) {
          const auto& sequences = received_per_producer[producer_id];
          EXPECT_EQ(sequences.size(), kItemsPerProducer)
              << "producer " << producer_id << " item count mismatch";

          for (size_t i = 0; i < sequences.size(); ++i) {
            EXPECT_EQ(sequences[i], i)
                << "producer " << producer_id << " sequence break at position " << i;
          }
        }
      },
      std::chrono::seconds(30));
}

TEST(LockFreeMpscRingCloseStressTest, CloseWithConcurrentPushAndDrain) {
  constexpr size_t kIterations = 50;
  constexpr size_t kProducerCount = 4;
  constexpr size_t kItemsPerProducer = 10'000;
  constexpr size_t kExpectedTotal = kProducerCount * kItemsPerProducer;

  for (size_t iter = 0; iter < kIterations; ++iter) {
    sfc::LockFreeMpscRing<size_t> queue(64, sfc::FullPolicy::Block);
    std::atomic<size_t> pushed_count{0};
    std::atomic<bool> start{false};
    std::vector<std::vector<size_t>> pushed_per_producer(kProducerCount);
    std::vector<std::mutex> pushed_mutexes(kProducerCount);

    sfc::test::RunWithHangGuardOrFail(
        [&](std::atomic<bool>& stop) {
          std::vector<std::jthread> producers;
          producers.reserve(kProducerCount);

          for (size_t producer_id = 0; producer_id < kProducerCount; ++producer_id) {
            producers.emplace_back([&, producer_id] {
              while (!start.load(std::memory_order_acquire) &&
                     !stop.load(std::memory_order_acquire)) {
                std::this_thread::yield();
              }
              for (size_t i = 0; i < kItemsPerProducer && !stop.load(std::memory_order_acquire);
                   ++i) {
                size_t item = producer_id * kItemsPerProducer + i;
                sfc::PushStatus status = queue.Push(item);
                if (status == sfc::PushStatus::Pushed) {
                  pushed_count.fetch_add(1, std::memory_order_relaxed);
                  std::lock_guard lock(pushed_mutexes[producer_id]);
                  pushed_per_producer[producer_id].push_back(item);
                } else if (status == sfc::PushStatus::Closed) {
                  return;
                }
              }
            });
          }

          std::jthread closer([&] {
            while (!start.load(std::memory_order_acquire) &&
                   !stop.load(std::memory_order_acquire)) {
              std::this_thread::yield();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            queue.Close();
          });

          start.store(true, std::memory_order_release);

          std::vector<size_t> received;
          received.reserve(kExpectedTotal);

          while (!stop.load(std::memory_order_acquire)) {
            sfc::PopResult<size_t> result = queue.TryPop();
            if (result.status == sfc::PopStatus::Item) {
              if (!result.value.has_value()) {
                ADD_FAILURE() << "iter " << iter << ": PopStatus::Item but no value";
                queue.Close();
                return;
              }
              received.push_back(*result.value);
            } else if (result.status == sfc::PopStatus::Closed) {
              break;
            } else {
              std::this_thread::yield();
            }
          }

          producers.clear();
          closer.join();

          // Build expected multiset from all pushed items.
          std::vector<size_t> expected;
          for (size_t producer_id = 0; producer_id < kProducerCount; ++producer_id) {
            expected.insert(expected.end(), pushed_per_producer[producer_id].begin(),
                            pushed_per_producer[producer_id].end());
          }

          size_t final_pushed = pushed_count.load(std::memory_order_relaxed);
          EXPECT_EQ(received.size(), final_pushed)
              << "iter " << iter << ": pushed " << final_pushed << " but received "
              << received.size();
          EXPECT_EQ(expected.size(), final_pushed)
              << "iter " << iter << ": expected size " << expected.size()
              << " != pushed count " << final_pushed;

          // Verify exact multiset match.
          std::sort(received.begin(), received.end());
          std::sort(expected.begin(), expected.end());
          EXPECT_EQ(received, expected) << "iter " << iter << ": received != expected";

          // Check for duplicates.
          for (size_t i = 1; i < received.size(); ++i) {
            if (received[i] == received[i - 1]) {
              ADD_FAILURE() << "iter " << iter << ": duplicate item " << received[i]
                           << " at positions " << (i - 1) << " and " << i;
              queue.Close();
              break;
            }
          }
        },
        std::chrono::seconds(10));
  }
}

