#include "sfc/version.h"
#include <gtest/gtest.h>

TEST(VersionTest, ReturnsExpectedVersion) {
    std::string version = sfc::Version();
    EXPECT_EQ(version, "0.1.0");
}
