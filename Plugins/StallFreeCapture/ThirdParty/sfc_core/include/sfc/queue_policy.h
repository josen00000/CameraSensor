#pragma once

#include <optional>

namespace sfc {

// What a bounded queue does when Push finds no free slot.
//
// DropOldest: evict the oldest stored item, then store the new one.
// Block: wait until a slot is free or the queue is closed.
// Reject: leave the queue unchanged.
enum class FullPolicy {
  DropOldest,
  Block,
  Reject,
};

// Result of one Push. Drop counts live on the queue, not here.
//
// Pushed: the item is stored. DropOldest may have evicted one older item.
// Rejected: the queue was full and the policy is Reject. The item was not stored.
// Closed: the queue is closed. The item was not stored. A producer blocked
//         under FullPolicy::Block also returns Closed when Close wakes it.
enum class PushStatus {
  Pushed,
  Rejected,
  Closed,
};

// Result of one TryPop. TryPop never waits.
//
// Item: value holds the oldest stored item. This still happens after Close
//       while items remain.
// Empty: the queue is open and holds nothing.
// Closed: the queue is closed and holds nothing.
enum class PopStatus {
  Item,
  Empty,
  Closed,
};

template <typename T>
struct PopResult {
  PopStatus status = PopStatus::Empty;
  std::optional<T> value;
};

}  // namespace sfc
