#include "sfc/latency_histogram.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sfc {

LatencyHistogram::LatencyHistogram() {
  for (auto& bucket : buckets_) {
    bucket.store(0, std::memory_order_relaxed);
  }
}

size_t LatencyHistogram::BucketIndex(uint64_t microseconds) {
  if (microseconds == 0) {
    return 0;
  }
  if (microseconds > kMaxRepresentableUs) {
    return kOverflowBucketIndex;
  }

  size_t index = 0;
  while (index + 1 < kNumBuckets &&
         microseconds >= kLowerBounds[index + 1]) {
    ++index;
  }
  return index;
}

void LatencyHistogram::Record(uint64_t microseconds) {
  const size_t index = BucketIndex(microseconds);
  buckets_[index].fetch_add(1, std::memory_order_relaxed);
  total_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t LatencyHistogram::Count() const {
  return total_.load(std::memory_order_relaxed);
}

void LatencyHistogram::Reset() {
  for (auto& bucket : buckets_) {
    bucket.store(0, std::memory_order_relaxed);
  }
  total_.store(0, std::memory_order_relaxed);
}

uint64_t LatencyHistogram::Percentile(double percentile) const {
  const uint64_t count = Count();
  if (count == 0) {
    return 0;
  }

  const double clamped =
      std::clamp(percentile, 0.0, 100.0);
  const double rank_real =
      std::ceil(clamped / 100.0 * static_cast<double>(count));
  uint64_t rank = static_cast<uint64_t>(rank_real);
  if (rank < 1) {
    rank = 1;
  }
  if (rank > count) {
    rank = count;
  }

  uint64_t cumulative = 0;
  for (size_t i = 0; i <= kOverflowBucketIndex; ++i) {
    cumulative += buckets_[i].load(std::memory_order_relaxed);
    if (cumulative >= rank) {
      if (i == kOverflowBucketIndex) {
        return kMaxRepresentableUs + 1;
      }
      return BucketLowerBound(i);
    }
  }

  return BucketLowerBound(kNumBuckets - 1);
}

}  // namespace sfc
