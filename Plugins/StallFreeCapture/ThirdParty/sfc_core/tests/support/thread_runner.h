#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace sfc::test {

// Runs `body(worker_index)` on `worker_count` std::thread workers and joins every
// worker before returning, including after exceptions or failed gtest assertions.
inline void RunOnThreads(std::size_t worker_count,
                           const std::function<void(std::size_t)>& body) {
  std::vector<std::thread> workers;
  workers.reserve(worker_count);

  std::mutex error_mutex;
  std::vector<std::string> worker_errors;

  std::mutex failure_mutex;
  std::vector<std::string> captured_failure_messages;

  for (std::size_t index = 0; index < worker_count; ++index) {
    workers.emplace_back([&, index] {
      testing::TestPartResultArray failures;
      testing::ScopedFakeTestPartResultReporter reporter(
          testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
          &failures);
      try {
        body(index);
      } catch (const testing::internal::GoogleTestFailureException&) {
        // ASSERT in the worker; captured below.
      } catch (const std::exception& exception) {
        std::lock_guard<std::mutex> lock(error_mutex);
        worker_errors.push_back(exception.what());
      } catch (...) {
        std::lock_guard<std::mutex> lock(error_mutex);
        worker_errors.push_back("unknown exception in worker");
      }

      if (failures.size() > 0) {
        std::lock_guard<std::mutex> lock(failure_mutex);
        for (int failure_index = 0; failure_index < failures.size();
             ++failure_index) {
          captured_failure_messages.push_back(
              failures.GetTestPartResult(failure_index).summary());
        }
      }
    });
  }

  for (std::thread& worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  for (const std::string& message : captured_failure_messages) {
    ADD_FAILURE() << message;
  }

  for (const std::string& message : worker_errors) {
    ADD_FAILURE() << message;
  }
}

}  // namespace sfc::test
