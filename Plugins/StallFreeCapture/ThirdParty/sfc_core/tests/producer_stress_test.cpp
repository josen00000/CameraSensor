#include "sfc/mutex_queue.h"

#include "support/hang_guard.h"
#include "support/thread_runner.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>

#include <gtest/gtest.h>

namespace {

TEST(ProducerStressTest, FourProducersOneConsumerEachItemOnce) {
  constexpr std::size_t kProducerCount = 4;
  constexpr int kItemsPerProducer = 250;
  constexpr int kTotalItems =
      static_cast<int>(kProducerCount) * kItemsPerProducer;

  sfc::test::RunWithHangGuardOrFail([&](std::atomic<bool>& stop) {
    sfc::MutexBoundedQueue<int> queue(32, sfc::FullPolicy::Block);

    std::array<std::atomic<int>, kTotalItems> receive_counts{};
    for (std::atomic<int>& count : receive_counts) {
      count.store(0, std::memory_order_relaxed);
    }

    std::atomic<int> consumed_items{0};
    std::atomic<bool> producers_finished{false};

    std::thread consumer([&] {
      while (true) {
        sfc::PopResult<int> result = queue.TryPop();
        if (result.status == sfc::PopStatus::Item) {
          const int item = result.value.value();
          ASSERT_GE(item, 0);
          ASSERT_LT(item, kTotalItems);
          receive_counts[static_cast<std::size_t>(item)].fetch_add(
              1, std::memory_order_relaxed);
          consumed_items.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        if (result.status == sfc::PopStatus::Closed) {
          break;
        }
        if (producers_finished.load(std::memory_order_acquire) &&
            consumed_items.load(std::memory_order_acquire) >= kTotalItems) {
          break;
        }
        if (stop.load(std::memory_order_acquire)) {
          break;
        }
        std::this_thread::yield();
      }
    });

    sfc::test::RunOnThreads(kProducerCount, [&](std::size_t producer_index) {
      for (int item_offset = 0; item_offset < kItemsPerProducer; ++item_offset) {
        if (stop.load(std::memory_order_acquire)) {
          return;
        }
        const int item =
            static_cast<int>(producer_index) * kItemsPerProducer + item_offset;
        while (true) {
          if (stop.load(std::memory_order_acquire)) {
            return;
          }
          const sfc::PushStatus status = queue.Push(item);
          if (status == sfc::PushStatus::Pushed) {
            break;
          }
          if (status == sfc::PushStatus::Closed) {
            return;
          }
        }
      }
    });

    producers_finished.store(true, std::memory_order_release);
    queue.Close();

    if (consumer.joinable()) {
      consumer.join();
    }

    EXPECT_EQ(consumed_items.load(), kTotalItems);
    for (int item = 0; item < kTotalItems; ++item) {
      EXPECT_EQ(receive_counts[static_cast<std::size_t>(item)].load(), 1)
          << "item id " << item;
    }
  });
}

}  // namespace
