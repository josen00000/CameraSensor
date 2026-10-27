#include "support/hang_guard.h"
#include "support/thread_runner.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace {

void RunOnThreadsJoinsAfterGtestFailureInWorkerBody() {
  std::atomic<int> joined_count{0};
  sfc::test::RunOnThreads(2, [&](std::size_t worker_index) {
    joined_count.fetch_add(1, std::memory_order_relaxed);
    if (worker_index == 0) {
      EXPECT_TRUE(false) << "intentional worker assertion";
    }
  });
  EXPECT_EQ(joined_count.load(), 2);
}

TEST(ConcurrencyHelpersTest, RunOnThreadsJoinsAfterWorkerException) {
  std::atomic<int> joined_count{0};

  EXPECT_NONFATAL_FAILURE(
      sfc::test::RunOnThreads(2, [&](std::size_t index) {
        joined_count.fetch_add(1, std::memory_order_relaxed);
        if (index == 0) {
          throw std::runtime_error("worker failure");
        }
      }),
      "worker failure");

  EXPECT_EQ(joined_count.load(), 2);
}

TEST(ConcurrencyHelpersTest, RunOnThreadsJoinsAfterGtestFailureInWorker) {
  EXPECT_NONFATAL_FAILURE(RunOnThreadsJoinsAfterGtestFailureInWorkerBody(),
                          "intentional worker assertion");
}

TEST(ConcurrencyHelpersTest, HangGuardTimesOutCooperativeBody) {
  const auto started = std::chrono::steady_clock::now();

  const sfc::test::HangGuardResult result = sfc::test::RunWithHangGuard(
      [](std::atomic<bool>& stop) {
        while (!stop.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
      },
      std::chrono::seconds(2));

  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                            started);

  EXPECT_EQ(result, sfc::test::HangGuardResult::TimedOut);
  EXPECT_GE(elapsed.count(), 1800);
  EXPECT_LT(elapsed.count(), 5000);
}

TEST(ConcurrencyHelpersTest, HangGuardCompletesFastBody) {
  const sfc::test::HangGuardResult result = sfc::test::RunWithHangGuard(
      [](std::atomic<bool>& stop) {
        (void)stop;
      },
      std::chrono::seconds(2));

  EXPECT_EQ(result, sfc::test::HangGuardResult::Completed);
}

}  // namespace
