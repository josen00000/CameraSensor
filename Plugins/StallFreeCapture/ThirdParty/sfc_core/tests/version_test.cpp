#include "sfc/version.h"
#include <gtest/gtest.h>

TEST(VersionTest, ReturnsNonEmptyVersion) {
    std::string version = sfc::Version();
    EXPECT_FALSE(version.empty());
}
