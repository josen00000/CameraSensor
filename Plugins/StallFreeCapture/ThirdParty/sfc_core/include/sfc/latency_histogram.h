#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sfc {

// Thread-safe fixed-bucket latency histogram (microseconds).
//
// Bucket layout (log-linear)
// --------------------------
// There are kNumBuckets finite buckets plus one overflow bucket. Finite buckets
// cover the closed interval [1 µs, 10 s] = [1, 10'000'000 µs]. Bucket i (0-based)
// holds samples in [lower(i), upper(i)), except the last finite bucket which is
// [lower(kNumBuckets - 1), 10'000'000] inclusive. The overflow bucket holds
// samples strictly greater than 10'000'000 µs.
//
// Lower bounds follow a geometric progression: lower(i) ≈ 10^(7·i/64) µs (rounded
// to integer, strictly increasing). Successive finite bucket widths grow with the
// value scale; each width is at most the width of the bucket that contains the
// true percentile on a sorted sample (see accuracy bound below).
//
// Percentile accuracy
// -------------------
// Percentile(p) uses the nearest-rank method on the aggregated histogram: for
// count N and percentile p ∈ [0, 100], the target rank is
//   rank = min(N, max(1, ⌈p/100 · N⌉))   (1-based)
// The returned value is the lower bound of the bucket where the cumulative count
// first reaches rank. For any fixed multiset of samples, each reported percentile
// differs from the nearest-rank value of the sorted samples by at most one
// finite-bucket width (the width of the bucket that contains the true rank value).
//
// Exception-free; safe to include from Unreal targets compiled with exceptions
// disabled.
class LatencyHistogram {
 public:
  static constexpr size_t kNumBuckets = 64;
  static constexpr uint64_t kMaxRepresentableUs = 10'000'000;  // 10 s
  static constexpr size_t kOverflowBucketIndex = kNumBuckets;

  LatencyHistogram();

  void Record(uint64_t microseconds);
  uint64_t Percentile(double percentile) const;
  uint64_t Count() const;
  void Reset();

  static constexpr uint64_t BucketLowerBound(size_t bucket_index);
  static constexpr uint64_t BucketUpperBound(size_t bucket_index);
  static constexpr uint64_t BucketWidth(size_t bucket_index);

 private:
  static constexpr std::array<uint64_t, kNumBuckets + 1> kLowerBounds = {
      1,     2,     3,     4,     5,     6,     7,     8,     9,     10,    12,
      16,    21,    26,    34,    44,    56,    72,    93,    120,   154,   198,
      255,   328,   422,   542,   698,   898,   1155,  1486,  1911,  2458,  3162,
      4068,  5233,  6732,  8660,  11140, 14330, 18434, 23714, 30505, 39242, 50481,
      64938, 83536, 107461, 138237, 177828, 228757, 294273, 378552, 486968, 626434,
      805842, 1036633, 1333521, 1715438, 2206734, 2838736, 3651741, 4697589, 6042964,
      7773650, kMaxRepresentableUs};

  static size_t BucketIndex(uint64_t microseconds);

  std::array<std::atomic<uint64_t>, kNumBuckets + 1> buckets_;
  std::atomic<uint64_t> total_{0};
};

constexpr uint64_t LatencyHistogram::BucketLowerBound(size_t bucket_index) {
  return kLowerBounds[bucket_index];
}

constexpr uint64_t LatencyHistogram::BucketUpperBound(size_t bucket_index) {
  if (bucket_index < kNumBuckets - 1) {
    return kLowerBounds[bucket_index + 1];
  }
  if (bucket_index == kNumBuckets - 1) {
    return kMaxRepresentableUs + 1;
  }
  return UINT64_MAX;
}

constexpr uint64_t LatencyHistogram::BucketWidth(size_t bucket_index) {
  const uint64_t upper = BucketUpperBound(bucket_index);
  const uint64_t lower = BucketLowerBound(bucket_index);
  return upper - lower;
}

}  // namespace sfc
