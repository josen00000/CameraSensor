#include "sfc/latency_histogram.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

void ExpectSingleSampleMapsTo(sfc::LatencyHistogram& histogram, uint64_t microseconds,
                              uint64_t expected_lower_bound) {
  histogram.Reset();
  histogram.Record(microseconds);
  EXPECT_EQ(histogram.Count(), 1u);
  EXPECT_EQ(histogram.Percentile(50.0), expected_lower_bound);
}

}  // namespace

TEST(LatencyHistogramTest, BucketBoundaryAssignments) {
  sfc::LatencyHistogram histogram;

  ExpectSingleSampleMapsTo(histogram, 0, 1);
  ExpectSingleSampleMapsTo(histogram, 1, 1);
  ExpectSingleSampleMapsTo(histogram, 2, 2);  // lower(1)

  // Hard-coded rows: value, expected bucket index, expected lower bound.
  const struct {
    uint64_t microseconds;
    size_t expected_bucket;
    uint64_t expected_lower;
  } kCases[] = {
      {9, 8, 9},
      {10, 9, 10},
      {11, 9, 10},
      {12, 10, 12},
      {55, 15, 44},
      {56, 16, 56},
      {sfc::LatencyHistogram::kMaxRepresentableUs, 63, 7'773'650},
  };

  for (const auto& case_row : kCases) {
    histogram.Reset();
    histogram.Record(case_row.microseconds);
    EXPECT_EQ(histogram.Count(), 1u);
    EXPECT_EQ(sfc::LatencyHistogram::BucketLowerBound(case_row.expected_bucket),
              case_row.expected_lower);
    EXPECT_EQ(histogram.Percentile(50.0), case_row.expected_lower);
  }

  histogram.Reset();
  histogram.Record(sfc::LatencyHistogram::kMaxRepresentableUs + 1);
  EXPECT_EQ(histogram.Count(), 1u);
  EXPECT_EQ(histogram.Percentile(50.0),
            sfc::LatencyHistogram::kMaxRepresentableUs + 1);
}

TEST(LatencyHistogramTest, PercentileFiftyJustBelowBoundaryReturnsLowerBound) {
  sfc::LatencyHistogram histogram;
  for (int i = 0; i < 99; ++i) {
    histogram.Record(11);
  }
  histogram.Record(1);

  EXPECT_EQ(histogram.Count(), 100u);
  EXPECT_EQ(histogram.Percentile(50.0), 10u);
}

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

  constexpr uint64_t kP50Reference = 2584;
  constexpr uint64_t kP50Reported = 2458;
  constexpr uint64_t kP50Tolerance = 704;

  constexpr uint64_t kP99Reference = 9'227'465;
  constexpr uint64_t kP99Reported = 7'773'650;
  constexpr uint64_t kP99Tolerance = 2'226'351;

  EXPECT_EQ(histogram.Percentile(50.0), kP50Reported);
  EXPECT_LE(kP50Reference - kP50Reported, kP50Tolerance);

  EXPECT_EQ(histogram.Percentile(99.0), kP99Reported);
  EXPECT_LE(kP99Reference - kP99Reported, kP99Tolerance);
}

TEST(LatencyHistogramTest, NanPercentileReturnsZero) {
  sfc::LatencyHistogram histogram;
  histogram.Record(100);
  EXPECT_EQ(histogram.Percentile(std::nan("")), 0u);
}

TEST(LatencyHistogramTest, ConcurrentRecordCountWithReaders) {
  sfc::LatencyHistogram histogram;
  constexpr int kThreads = 4;
  constexpr int kRecordsPerThread = 1'000'000;

  std::atomic<bool> writers_done{false};
  std::thread reader([&histogram, &writers_done]() {
    while (!writers_done.load(std::memory_order_acquire)) {
      histogram.Count();
      histogram.Percentile(50.0);
      histogram.Percentile(99.0);
    }
    histogram.Count();
    histogram.Percentile(50.0);
  });

  std::vector<std::thread> writers;
  writers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    writers.emplace_back([&histogram]() {
      for (int i = 0; i < kRecordsPerThread; ++i) {
        histogram.Record(static_cast<uint64_t>((i % 50'000) + 1));
      }
    });
  }
  for (std::thread& thread : writers) {
    thread.join();
  }
  writers_done.store(true, std::memory_order_release);
  reader.join();

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
