#pragma once

#include "sfc/queue_policy.h"

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

namespace sfc {

// Bounded FIFO queue guarded by one mutex.
//
// Logical capacity is the value passed to the constructor. capacity >= 1.
// The policy is fixed for the life of the queue.
//
// Close() is shutdown: later Push calls refuse the new item, items already
// stored can still be popped, and every producer waiting in Block wakes.
// Destroy the queue only after concurrent Push, TryPop, and Close calls
// have returned. The destructor does not call Close().
//
// Several producers and several consumers may call the queue at once.
// Copy and move are unavailable because the queue owns a mutex.
template <typename T>
class MutexBoundedQueue {
 public:
  explicit MutexBoundedQueue(size_t capacity, FullPolicy policy)
      : capacity_(capacity), policy_(policy) {
    assert(capacity >= 1);
  }

  MutexBoundedQueue(const MutexBoundedQueue&) = delete;
  MutexBoundedQueue& operator=(const MutexBoundedQueue&) = delete;
  MutexBoundedQueue(MutexBoundedQueue&&) = delete;
  MutexBoundedQueue& operator=(MutexBoundedQueue&&) = delete;

  // Move or copy the argument into the queue only when returning Pushed.
  // On Rejected or Closed the caller still owns it.
  template <typename U>
  PushStatus Push(U&& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (closed_) {
      return PushStatus::Closed;
    }
    if (items_.size() == capacity_) {
      if (policy_ == FullPolicy::Reject) {
        return PushStatus::Rejected;
      }
      if (policy_ == FullPolicy::DropOldest) {
        items_.pop_front();
        ++drop_count_;
      } else {
        cv_.wait(lock, [this] { return closed_ || items_.size() < capacity_; });
        if (closed_) {
          return PushStatus::Closed;
        }
      }
    }
    items_.push_back(std::forward<U>(item));
    return PushStatus::Pushed;
  }

  // Never waits. See PopStatus.
  PopResult<T> TryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.empty()) {
      PopResult<T> result;
      result.status = closed_ ? PopStatus::Closed : PopStatus::Empty;
      return result;
    }
    PopResult<T> result;
    result.status = PopStatus::Item;
    result.value = std::move(items_.front());
    items_.pop_front();
    cv_.notify_one();
    return result;
  }

  void Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    cv_.notify_all();
  }

  uint64_t DropCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return drop_count_;
  }

  size_t Capacity() const { return capacity_; }

  bool IsClosed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<T> items_;
  size_t capacity_;
  FullPolicy policy_;
  bool closed_ = false;
  uint64_t drop_count_ = 0;
};

}  // namespace sfc
