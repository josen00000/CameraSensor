#include "sfc/latency_histogram.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

uint64_t NearestRankValue(const std::vector<uint64_t>& sorted, double percentile) {
  const size_t count = sorted.size();
  if (count == 0) {
    return 0;
  }
  const double clamped = std::clamp(percentile, 0.0, 100.0);
  const double rank_real =
      std::ceil(clamped / 100.0 * static_cast<double>(count));
  uint64_t rank = static_cast<uint64_t>(rank_real);
  if (rank < 1) {
    rank = 1;
  }
  if (rank > count) {
    rank = count;
  }
  return sorted[rank - 1];
}

size_t BucketIndexForValue(uint64_t value) {
  if (value == 0) {
    return 0;
  }
  if (value > sfc::LatencyHistogram::kMaxRepresentableUs) {
    return sfc::LatencyHistogram::kOverflowBucketIndex;
  }
  size_t index = 0;
  while (index + 1 < sfc::LatencyHistogram::kNumBuckets &&
         value >= sfc::LatencyHistogram::BucketLowerBound(index + 1)) {
    ++index;
  }
  return index;
}

uint64_t BucketWidthForValue(uint64_t value) {
  if (value > sfc::LatencyHistogram::kMaxRepresentableUs) {
    return std::numeric_limits<uint64_t>::max();
  }
  const size_t index = BucketIndexForValue(value);
  return sfc::LatencyHistogram::BucketWidth(index);
}

void ExpectPercentileNearReference(sfc::LatencyHistogram& histogram,
                                  const std::vector<uint64_t>& samples,
                                  double percentile) {
  std::vector<uint64_t> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  const uint64_t reference = NearestRankValue(sorted, percentile);
  const uint64_t reported = histogram.Percentile(percentile);
  const uint64_t tolerance = BucketWidthForValue(reference);

  const uint64_t delta =
      (reported >= reference) ? (reported - reference) : (reference - reported);
  EXPECT_LE(delta, tolerance)
      << "p=" << percentile << " ref=" << reference << " got=" << reported
      << " tolerance=" << tolerance;
}

}  // namespace

TEST(LatencyHistogramTest, PercentilesWithinOneBucketWidth) {
  const std::vector<uint64_t> samples = {
      1,     2,     3,     5,     8,      13,     21,     34,     55,
      89,    144,   233,   377,   610,    987,    1597,   2584,   4181,
      6765,  10946, 17711, 28657, 46368,  75025,  121393, 196418, 317811,
      514229, 832040, 1346269, 2178309, 3524578, 5702887, 9227465};

  sfc::LatencyHistogram histogram;
  for (const uint64_t sample : samples) {
    histogram.Record(sample);
  }

  ExpectPercentileNearReference(histogram, samples, 50.0);
  ExpectPercentileNearReference(histogram, samples, 99.0);
}

TEST(LatencyHistogramTest, ConcurrentRecordCount) {
  sfc::LatencyHistogram histogram;
  constexpr int kThreads = 4;
  constexpr int kRecordsPerThread = 1'000'000;

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&histogram]() {
      for (int i = 0; i < kRecordsPerThread; ++i) {
        histogram.Record(static_cast<uint64_t>((i % 50'000) + 1));
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(histogram.Count(), static_cast<uint64_t>(kThreads * kRecordsPerThread));
}

TEST(LatencyHistogramTest, OverflowBucketIncludesSixtySeconds) {
  sfc::LatencyHistogram histogram;
  constexpr uint64_t kSixtySecondsUs = 60'000'000;

  histogram.Record(kSixtySecondsUs);

  EXPECT_EQ(histogram.Count(), 1u);
  EXPECT_EQ(histogram.Percentile(50.0), sfc::LatencyHistogram::kMaxRepresentableUs + 1);
}

TEST(LatencyHistogramTest, ResetClearsCounts) {
  sfc::LatencyHistogram histogram;
  histogram.Record(42);
  histogram.Record(60'000'000);
  EXPECT_EQ(histogram.Count(), 2u);

  histogram.Reset();
  EXPECT_EQ(histogram.Count(), 0u);
  EXPECT_EQ(histogram.Percentile(50.0), 0u);
}
