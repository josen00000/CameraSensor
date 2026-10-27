#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

namespace sfc::test {

inline constexpr std::chrono::milliseconds kHangGuardJoinGrace{500};

enum class HangGuardResult {
  Completed,
  TimedOut,
};

namespace detail {

inline void ReportPostTimeoutWorkerException(const std::exception_ptr& worker_exception) {
  if (!worker_exception) {
    return;
  }
  try {
    std::rethrow_exception(worker_exception);
  } catch (const std::exception& exception) {
    std::cerr << "sfc::test::HangGuard: worker exception after timeout: "
              << exception.what() << '\n';
  } catch (...) {
    std::cerr << "sfc::test::HangGuard: unknown worker exception after timeout\n";
  }
}

inline void AbortNonCooperativeHang(std::chrono::milliseconds timeout) {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  const char* const test_name = test_info != nullptr ? test_info->name() : "unknown";
  std::cerr << "sfc::test::HangGuard: non-cooperative timeout in test " << test_name
            << " after "
            << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()
            << " ms (join grace "
            << std::chrono::duration_cast<std::chrono::milliseconds>(kHangGuardJoinGrace)
                   .count()
            << " ms exhausted)\n";
  std::abort();
}

inline bool WaitUntilFinishedOrJoinGrace(
    const std::atomic<bool>& finished,
    std::chrono::steady_clock::time_point grace_deadline) {
  while (!finished.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= grace_deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

}  // namespace detail

// Runs `body(stop)` on a worker thread. If it does not finish within `timeout`,
// sets `stop` and waits up to kHangGuardJoinGrace for cooperative exit. Bodies that
// ignore `stop` are terminated with std::abort() after that grace period.
inline HangGuardResult RunWithHangGuard(
    const std::function<void(std::atomic<bool>& stop)>& body,
    std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  std::atomic<bool> stop{false};
  std::atomic<bool> finished{false};

  std::exception_ptr worker_exception;

  std::thread worker([&] {
    try {
      body(stop);
    } catch (const testing::internal::GoogleTestFailureException&) {
      // ASSERT failures use this internal type; gtest records them on the worker thread.
    } catch (...) {
      worker_exception = std::current_exception();
    }
    finished.store(true, std::memory_order_release);
  });

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!finished.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      stop.store(true, std::memory_order_release);
      const auto grace_deadline =
          std::chrono::steady_clock::now() + kHangGuardJoinGrace;
      if (!detail::WaitUntilFinishedOrJoinGrace(finished, grace_deadline)) {
        detail::AbortNonCooperativeHang(timeout);
      }
      if (worker.joinable()) {
        worker.join();
      }
      detail::ReportPostTimeoutWorkerException(worker_exception);
      return HangGuardResult::TimedOut;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (worker.joinable()) {
    worker.join();
  }
  if (worker_exception) {
    std::rethrow_exception(worker_exception);
  }
  return HangGuardResult::Completed;
}

// Same as RunWithHangGuard, but records a gtest failure when the body times out.
inline void RunWithHangGuardOrFail(
    const std::function<void(std::atomic<bool>& stop)>& body,
    std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
  const HangGuardResult result = RunWithHangGuard(body, timeout);
  if (result == HangGuardResult::TimedOut) {
    ADD_FAILURE() << "hang guard timed out after "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()
                  << " ms";
  }
}

}  // namespace sfc::test
