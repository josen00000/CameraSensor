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
// samples strictly greater than 10'000'000 µs. A sample of 0 µs is recorded in
// bucket 0 (same as values in [1, lower(1))).
//
// Lower bounds follow a geometric progression: lower(i) ≈ 10^(7·i/64) µs (rounded
// to integer, strictly increasing). Relative bucket width (upper − lower) / lower
// is about 29% on average for i ≥ 10; the widest low decade step is 33% at
// [12, 16) µs. Buckets 0–9 (1–10 µs) each have width 1 (0% relative error from
// width alone).
//
// Concurrency
// -----------
// Record() is safe to call concurrently. Percentile() and Count() each load all
// bucket atomics once; results are a consistent snapshot of those loads but may
// not correspond to any single point in time when other threads are recording.
// Do not call Reset() concurrently with Record().
//
// Percentile accuracy
// -------------------
// Percentile(p) uses the nearest-rank method on the snapshot: for count N and
// percentile p ∈ [0, 100] (NaN p returns 0), the target rank is
//   rank = min(N, max(1, ⌈p/100 · N⌉))   (1-based)
// The returned value is the lower bound of the finite bucket where the cumulative
// count first reaches rank, or 10'000'001 µs when that bucket is overflow. An
// empty histogram (N = 0) returns 0.
//
// Compared with the exact nearest-rank value from sorting the same samples, the
// error is strictly less than that bucket's width (upper − lower): the answer is
// always the true bucket's lower bound, which is at most the true sample value
// and at least (true value − width + 1) for finite buckets. Overflow percentiles
// report 10'000'001 with no finite width bound.
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
