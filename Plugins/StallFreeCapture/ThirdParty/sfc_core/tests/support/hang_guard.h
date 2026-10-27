#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

namespace sfc::test {

enum class HangGuardResult {
  Completed,
  TimedOut,
};

// Runs `body(stop)` on a worker thread. If it does not finish within `timeout`,
// sets `stop` and waits for the worker to exit cooperatively, then returns TimedOut.
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
      // Recorded by gtest; still mark finished so the guard can join.
    } catch (...) {
      worker_exception = std::current_exception();
    }
    finished.store(true, std::memory_order_release);
  });

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!finished.load(std::memory_order_acquire)) {
    if (std::chrono::steady_clock::now() >= deadline) {
      stop.store(true, std::memory_order_release);
      if (worker.joinable()) {
        worker.join();
      }
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
