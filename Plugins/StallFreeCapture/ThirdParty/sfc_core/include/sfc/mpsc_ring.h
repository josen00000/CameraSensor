#pragma once

#include "sfc/queue_policy.h"

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>

namespace sfc {

// Lock-free bounded FIFO MPSC (multiple producer, single consumer) ring.
// Based on Dmitry Vyukov's bounded MPSC queue design with per-slot sequence atomics.
//
// Logical capacity is rounded up to the next power of two. capacity >= 1.
// The policy is fixed for the life of the queue. DropOldest is not implemented.
//
// Close() is shutdown: later Push calls refuse the new item, items already
// stored can still be popped, and every producer waiting in Block wakes.
// Destroy the queue only after concurrent Push, TryPop, and Close calls
// have returned. The destructor does not call Close().
//
// Several producers may call Push at once. Only one consumer may call TryPop.
// Copy and move are unavailable because the queue owns atomics.
template <typename T>
class LockFreeMpscRing {
 public:
  explicit LockFreeMpscRing(size_t capacity, FullPolicy policy)
      : logical_capacity_(capacity),
        buffer_capacity_(RoundUpPowerOfTwo(capacity)),
        mask_(buffer_capacity_ - 1),
        policy_(policy),
        buffer_(new Slot[buffer_capacity_]) {
    assert(capacity >= 1);
    assert(policy != FullPolicy::DropOldest &&
           "DropOldest not implemented for LockFreeMpscRing (see P3-T28)");

    for (size_t i = 0; i < buffer_capacity_; ++i) {
      buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    enqueue_pos_.store(0, std::memory_order_relaxed);
    dequeue_pos_.store(0, std::memory_order_relaxed);
  }

  ~LockFreeMpscRing() {
    size_t read_pos = dequeue_pos_.load(std::memory_order_relaxed);
    size_t write_pos = enqueue_pos_.load(std::memory_order_relaxed);

    for (size_t pos = read_pos; pos < write_pos; ++pos) {
      Slot& slot = buffer_[pos & mask_];
      DestroySlot(slot);
    }

    delete[] buffer_;
  }

  LockFreeMpscRing(const LockFreeMpscRing&) = delete;
  LockFreeMpscRing& operator=(const LockFreeMpscRing&) = delete;
  LockFreeMpscRing(LockFreeMpscRing&&) = delete;
  LockFreeMpscRing& operator=(LockFreeMpscRing&&) = delete;

  // Move or copy the argument into the queue only when returning Pushed.
  // On Rejected or Closed the caller still owns it.
  template <typename U>
  PushStatus Push(U&& item) {
    static_assert(std::is_same_v<std::decay_t<U>, T>,
                  "Push argument must match queue element type");

    if (closed_.load(std::memory_order_acquire)) {
      return PushStatus::Closed;
    }

    size_t backoff_count = 0;
    constexpr size_t kMaxSpins = 8;
    constexpr size_t kMaxYields = 16;

    for (;;) {
      if (closed_.load(std::memory_order_acquire)) {
        return PushStatus::Closed;
      }

      size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
      size_t dpos = dequeue_pos_.load(std::memory_order_acquire);
      
      if (pos - dpos >= logical_capacity_) {
        if (policy_ == FullPolicy::Reject) {
          return PushStatus::Rejected;
        }

        if (policy_ == FullPolicy::Block) {
          if (backoff_count < kMaxSpins) {
            ++backoff_count;
            for (size_t spin = 0; spin < (1u << (backoff_count / 8)); ++spin) {
              std::atomic_signal_fence(std::memory_order_acquire);
            }
          } else if (backoff_count < kMaxSpins + kMaxYields) {
            ++backoff_count;
            std::this_thread::yield();
          } else {
            uint32_t current = block_signal_.load(std::memory_order_acquire);
            if (closed_.load(std::memory_order_acquire)) {
              return PushStatus::Closed;
            }
            block_signal_.wait(current, std::memory_order_acquire);
          }
        }
        continue;
      }

      Slot& slot = buffer_[pos & mask_];
      size_t seq = slot.sequence.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

      if (diff == 0) {
        if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          ConstructSlot(slot, std::forward<U>(item));
          slot.sequence.store(pos + 1, std::memory_order_release);
          return PushStatus::Pushed;
        }
      } else if (diff < 0) {
        if (policy_ == FullPolicy::Reject) {
          return PushStatus::Rejected;
        }

        if (policy_ == FullPolicy::Block) {
          if (backoff_count < kMaxSpins) {
            ++backoff_count;
            for (size_t spin = 0; spin < (1u << (backoff_count / 8)); ++spin) {
              std::atomic_signal_fence(std::memory_order_acquire);
            }
          } else if (backoff_count < kMaxSpins + kMaxYields) {
            ++backoff_count;
            std::this_thread::yield();
          } else {
            uint32_t current = block_signal_.load(std::memory_order_acquire);
            if (closed_.load(std::memory_order_acquire)) {
              return PushStatus::Closed;
            }
            block_signal_.wait(current, std::memory_order_acquire);
          }
        }
      } else {
        // Sequence ahead: enqueue_pos_ wrapped, retry
      }
    }
  }

  // Never waits. See PopStatus.
  PopResult<T> TryPop() {
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    Slot& slot = buffer_[pos & mask_];
    size_t seq = slot.sequence.load(std::memory_order_acquire);
    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

    if (diff == 0) {
      PopResult<T> result;
      result.status = PopStatus::Item;
      result.value = MoveFromSlot(slot);
      DestroySlot(slot);
      slot.sequence.store(pos + buffer_capacity_, std::memory_order_release);
      dequeue_pos_.store(pos + 1, std::memory_order_release);
      block_signal_.fetch_add(1, std::memory_order_release);
      block_signal_.notify_all();
      return result;
    }

    PopResult<T> result;
    if (diff < 0) {
      result.status = closed_.load(std::memory_order_acquire) ? PopStatus::Closed
                                                               : PopStatus::Empty;
    } else {
      result.status = PopStatus::Empty;
    }
    return result;
  }

  void Close() {
    closed_.store(true, std::memory_order_release);
    block_signal_.fetch_add(1, std::memory_order_release);
    block_signal_.notify_all();
  }

  uint64_t DropCount() const { return 0; }

  size_t Capacity() const { return logical_capacity_; }

  bool IsClosed() const { return closed_.load(std::memory_order_acquire); }

 private:
  struct Slot {
    alignas(64) std::atomic<size_t> sequence;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
  };

  static size_t RoundUpPowerOfTwo(size_t n) {
    if (n <= 1) {
      return 2;
    }
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) > 4) {
      n |= n >> 32;
    }
    return n + 1;
  }

  template <typename U>
  void ConstructSlot(Slot& slot, U&& value) {
    new (&slot.storage) T(std::forward<U>(value));
  }

  T MoveFromSlot(Slot& slot) {
    T* ptr = reinterpret_cast<T*>(&slot.storage);
    return std::move(*ptr);
  }

  void DestroySlot(Slot& slot) {
    T* ptr = reinterpret_cast<T*>(&slot.storage);
    ptr->~T();
  }

  alignas(64) std::atomic<size_t> enqueue_pos_;
  alignas(64) std::atomic<size_t> dequeue_pos_;
  const size_t logical_capacity_;
  const size_t buffer_capacity_;
  const size_t mask_;
  const FullPolicy policy_;
  std::atomic<bool> closed_{false};
  std::atomic<uint32_t> block_signal_{0};
  Slot* buffer_;
};

}  // namespace sfc
