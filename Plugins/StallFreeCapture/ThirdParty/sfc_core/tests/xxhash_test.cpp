#include "sfc/xxhash_wrapper.h"

#include <gtest/gtest.h>
#include <cstring>

TEST(XxHashTest, ReferenceVectorEmpty) {
  const char* data = "";
  uint64_t hash = sfc::XxHash64(data, 0, 0);
  EXPECT_EQ(hash, 0xef46db3751d8e999ULL);
}

TEST(XxHashTest, ReferenceVectorShort) {
  const char* data = "test";
  uint64_t hash = sfc::XxHash64(data, strlen(data), 0);
  EXPECT_EQ(hash, 0x4fdcca5ddb678139ULL);
}

TEST(XxHashTest, ReferenceVectorLonger) {
  const char* data = "The quick brown fox jumps over the lazy dog";
  uint64_t hash = sfc::XxHash64(data, strlen(data), 0);
  EXPECT_EQ(hash, 0x0b242d361fda71bcULL);
}

TEST(XxHashTest, DifferentDataProducesDifferentHash) {
  const char* data1 = "test1";
  const char* data2 = "test2";

  uint64_t hash1 = sfc::XxHash64(data1, strlen(data1), 0);
  uint64_t hash2 = sfc::XxHash64(data2, strlen(data2), 0);

  EXPECT_NE(hash1, hash2);
}

TEST(XxHashTest, DifferentSeedsProduceDifferentHashes) {
  const char* data = "test";

  uint64_t hash1 = sfc::XxHash64(data, strlen(data), 0);
  uint64_t hash2 = sfc::XxHash64(data, strlen(data), 123);

  EXPECT_NE(hash1, hash2);
}
