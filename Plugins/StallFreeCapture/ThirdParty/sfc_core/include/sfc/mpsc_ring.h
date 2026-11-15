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
//
// Close correctness: Push increments inflight_ (seq_cst) before checking closed_
// (seq_cst), ensuring any Push that passes the closed_ check is counted. Close
// stores closed_ (seq_cst); TryPop returns Closed only if closed_ && inflight_==0
// && the slot is empty, guaranteeing no in-flight Push can lose an item.
template <typename T>
class LockFreeMpscRing {
 public:
  static_assert(std::is_nothrow_move_constructible_v<T>,
                "T must be nothrow move constructible to avoid stalling on exception");

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
      // Relaxed: initial setup, no concurrent access yet.
      buffer_[i].sequence.store(i, std::memory_order_relaxed);
    }

    // Relaxed: initial setup, no concurrent access yet.
    enqueue_pos_.store(0, std::memory_order_relaxed);
    dequeue_pos_.store(0, std::memory_order_relaxed);
    inflight_.store(0, std::memory_order_relaxed);
  }

  ~LockFreeMpscRing() {
    // Relaxed: destructor runs after all concurrent operations complete.
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
    static_assert(std::is_nothrow_constructible_v<T, U&&>,
                  "T must be nothrow constructible from U to avoid stalling on exception");

    // Seq_cst: participates in total order S with closed_.store in Close() and
    // inflight_.load / closed_.load in TryPop(); ensures any Push that increments
    // inflight_ before Close() stores closed_=true will complete or decrement
    // before TryPop can see closed_=true && inflight_=0.
    inflight_.fetch_add(1, std::memory_order_seq_cst);

    // Seq_cst: participates in total order S. In S, either this load precedes
    // Close's closed_.store (then TryPop sees inflight_>=1, or sees the seq_cst
    // decrement that follows the slot publish), or it follows that store (then
    // this Push sees closed_=true, decrements inflight_, and returns Closed).
    if (closed_.load(std::memory_order_seq_cst)) {
      // Relaxed: no synchronization needed; just decrement the count.
      inflight_.fetch_sub(1, std::memory_order_relaxed);
      return PushStatus::Closed;
    }

    size_t backoff_count = 0;
    constexpr size_t kMaxSpins = 8;
    constexpr size_t kMaxYields = 16;

    for (;;) {
      // Seq_cst: same as entry check; ensures no in-flight Push loses items.
      if (closed_.load(std::memory_order_seq_cst)) {
        // Relaxed: no synchronization needed; just decrement the count.
        inflight_.fetch_sub(1, std::memory_order_relaxed);
        return PushStatus::Closed;
      }

      // Snapshot block_signal_ BEFORE checking fullness to avoid lost wakeup:
      // a consumer/Close increment after this snapshot changes the value, so
      // wait() returns immediately; an increment before this snapshot is ordered
      // (release fetch_add -> acquire load), so the freed slot or closed flag
      // is visible to the re-check after wait() returns.
      // Acquire: forms acquire-release pair with block_signal_.fetch_add(release)
      // in TryPop() and Close(); ensures we see the queue state that triggered
      // that signal update (freed slot or closed flag).
      uint32_t signal_snapshot = block_signal_.load(std::memory_order_acquire);

      // Relaxed: the loaded value is only a guess that will be validated by
      // the CAS below; all producers update enqueue_pos_ via CAS.
      size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

      // Acquire: forms acquire-release pair with dequeue_pos_.store(release)
      // in TryPop(); ensures producer sees the consumer's updated dequeue
      // position, preventing false 'queue full' decisions.
      size_t dpos = dequeue_pos_.load(std::memory_order_acquire);

      if (pos - dpos >= logical_capacity_) {
        if (policy_ == FullPolicy::Reject) {
          // Relaxed: no synchronization needed; just decrement the count.
          inflight_.fetch_sub(1, std::memory_order_relaxed);
          return PushStatus::Rejected;
        }

        if (policy_ == FullPolicy::Block) {
          if (backoff_count < kMaxSpins) {
            ++backoff_count;
            // Exponential backoff: 2^1=2, 2^2=4, 2^3=8, ..., 2^8=256 spins.
            for (size_t spin = 0; spin < (size_t{1} << backoff_count); ++spin) {
              std::atomic_signal_fence(std::memory_order_acquire);
            }
          } else if (backoff_count < kMaxSpins + kMaxYields) {
            ++backoff_count;
            std::this_thread::yield();
          } else {
            // Recheck closed before sleeping to avoid waiting on a closed queue.
            // Seq_cst: same as entry check.
            if (closed_.load(std::memory_order_seq_cst)) {
              // Relaxed: no synchronization needed; just decrement the count.
              inflight_.fetch_sub(1, std::memory_order_relaxed);
              return PushStatus::Closed;
            }
            // Wait on the snapshot taken before the fullness check; if consumer
            // drained since the snapshot, the signal changed and wait returns
            // immediately, avoiding lost wakeup.
            block_signal_.wait(signal_snapshot, std::memory_order_relaxed);
          }
        }
        continue;
      }

      Slot& slot = buffer_[pos & mask_];

      // Acquire: forms acquire-release pair with slot.sequence.store(release)
      // in TryPop(); ensures producer sees consumer's data destruction before
      // reusing the slot, preventing use-after-free.
      size_t seq = slot.sequence.load(std::memory_order_acquire);
      intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

      if (diff == 0) {
        // Relaxed: CAS is coordination only; actual data synchronization
        // happens via slot.sequence store (release) below.
        if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed)) {
          ConstructSlot(slot, std::forward<U>(item));

          // Seq_cst: participates in total order S with inflight_.fetch_sub below,
          // inflight_.load and slot.sequence.load in TryPop(); in S, if TryPop's
          // inflight_.load observes 0, this store precedes it, so TryPop's later
          // slot.sequence.load sees the published item.
          slot.sequence.store(pos + 1, std::memory_order_seq_cst);

          // Seq_cst: participates in total order S with inflight_.load in TryPop()
          // and slot.sequence.store above; in S, if TryPop sees inflight_=0, all
          // slot publications whose decrements preceded that load are visible.
          inflight_.fetch_sub(1, std::memory_order_seq_cst);
          return PushStatus::Pushed;
        }
      } else if (diff < 0) {
        if (policy_ == FullPolicy::Reject) {
          // Relaxed: no synchronization needed; just decrement the count.
          inflight_.fetch_sub(1, std::memory_order_relaxed);
          return PushStatus::Rejected;
        }

        if (policy_ == FullPolicy::Block) {
          if (backoff_count < kMaxSpins) {
            ++backoff_count;
            // Exponential backoff: 2^1=2, 2^2=4, 2^3=8, ..., 2^8=256 spins.
            for (size_t spin = 0; spin < (size_t{1} << backoff_count); ++spin) {
              std::atomic_signal_fence(std::memory_order_acquire);
            }
          } else if (backoff_count < kMaxSpins + kMaxYields) {
            ++backoff_count;
            std::this_thread::yield();
          } else {
            // Recheck closed before sleeping.
            // Seq_cst: same as entry check.
            if (closed_.load(std::memory_order_seq_cst)) {
              // Relaxed: no synchronization needed; just decrement the count.
              inflight_.fetch_sub(1, std::memory_order_relaxed);
              return PushStatus::Closed;
            }
            // Wait on signal_snapshot from before fullness check.
            block_signal_.wait(signal_snapshot, std::memory_order_relaxed);
          }
        }
      } else {
        // Sequence ahead: enqueue_pos_ wrapped, retry
      }
    }
  }

  // Never waits. See PopStatus.
  PopResult<T> TryPop() {
    // Check if we should return Closed BEFORE checking the slot; this ensures
    // we don't miss items that are being published while Close() is called.
    // Seq_cst: participates in total order S with closed_.store in Close() and
    // inflight_.fetch_add in Push(); Dekker-style: in S, if this load observes
    // closed_=true and a Push's closed_.load observes false, that Push's
    // inflight_.fetch_add precedes this load (prevents lost items at Close).
    bool check_closed = closed_.load(std::memory_order_seq_cst);

    // Seq_cst: participates in total order S with inflight_.fetch_sub in Push()
    // and closed_.store in Close(); in S, if this load observes inflight_=0,
    // all Push calls' slot publications and decrements precede this load.
    uint32_t check_inflight = inflight_.load(std::memory_order_seq_cst);

    // Now check the slot.
    // Relaxed: single consumer, only this thread writes dequeue_pos.
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    Slot& slot = buffer_[pos & mask_];

    // Seq_cst: participates in total order S with slot.sequence.store and
    // inflight_.fetch_sub in Push(); in S, if we saw inflight_=0 above, all
    // slot publications whose decrements precede that load are visible here.
    size_t seq = slot.sequence.load(std::memory_order_seq_cst);
    intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

    if (diff == 0) {
      PopResult<T> result;
      result.status = PopStatus::Item;
      result.value = MoveFromSlot(slot);
      DestroySlot(slot);

      // Release: forms acquire-release pair with slot.sequence.load(acquire)
      // in Push(); ensures data destruction happens-before producer reuses slot.
      slot.sequence.store(pos + buffer_capacity_, std::memory_order_release);

      // Release: forms acquire-release pair with dequeue_pos_.load(acquire)
      // in Push(); ensures updated dequeue_pos happens-before producer's
      // fullness check, unblocking waiting producers.
      dequeue_pos_.store(pos + 1, std::memory_order_release);

      // Release: forms acquire-release pair with block_signal_.load(acquire)
      // in Push(); signals blocked producers that space is available.
      block_signal_.fetch_add(1, std::memory_order_release);
      block_signal_.notify_all();
      return result;
    }

    // Slot is empty (diff < 0) or sequence is ahead (diff > 0); in both cases
    // no item is ready. Only return Closed if the queue is closed, no Push is
    // in flight, and the slot is empty (contract: Closed = closed && empty).
    // We checked closed_ and inflight_ above BEFORE checking the slot, so if
    // we saw closed_=true && inflight_=0, and now the slot is empty, it's safe
    // to return Closed (any Push that was in-flight is now complete).
    PopResult<T> result;
    if (diff < 0 && check_closed && check_inflight == 0) {
      result.status = PopStatus::Closed;
    } else {
      result.status = PopStatus::Empty;
    }
    return result;
  }

  void Close() {
    // Seq_cst: forms seq_cst pair with inflight_.fetch_add and closed_.load
    // in Push(); ensures any Push that increments inflight_ before this store
    // will complete or decrement before TryPop sees closed_=true && inflight_=0.
    closed_.store(true, std::memory_order_seq_cst);

    // Release: forms acquire-release pair with block_signal_.load(acquire)
    // in Push(); signals blocked producers to wake and check closed_.
    block_signal_.fetch_add(1, std::memory_order_release);
    block_signal_.notify_all();
  }

  uint64_t DropCount() const { return 0; }

  size_t Capacity() const { return logical_capacity_; }

  bool IsClosed() const {
    // Acquire: forms acquire-release pair with closed_.store(seq_cst) in Close();
    // ensures caller sees Close() and any prior state changes.
    return closed_.load(std::memory_order_acquire);
  }

 private:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
  struct Slot {
    alignas(64) std::atomic<size_t> sequence;
    alignas(alignof(T)) std::byte storage[sizeof(T)];
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
    T* ptr = std::launder(reinterpret_cast<T*>(&slot.storage));
    return std::move(*ptr);
  }

  void DestroySlot(Slot& slot) {
    T* ptr = std::launder(reinterpret_cast<T*>(&slot.storage));
    ptr->~T();
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
  alignas(64) std::atomic<size_t> enqueue_pos_;
  alignas(64) std::atomic<size_t> dequeue_pos_;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  const size_t logical_capacity_;
  const size_t buffer_capacity_;
  const size_t mask_;
  const FullPolicy policy_;
  std::atomic<bool> closed_{false};
  std::atomic<uint32_t> block_signal_{0};
  std::atomic<uint32_t> inflight_{0};
  Slot* buffer_;
};

}  // namespace sfc
