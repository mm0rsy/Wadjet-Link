#include <gtest/gtest.h>
#include <wadjet/version.hpp>
#include <cstring>

namespace wadjet::test {

TEST(Version, ConstantsAreDefined) {
    EXPECT_EQ(VERSION_MAJOR, 0);
    EXPECT_EQ(VERSION_MINOR, 1);
    EXPECT_EQ(VERSION_PATCH, 0);
}

TEST(Version, StringIsNotNull) {
    EXPECT_NE(version_string(), nullptr);
}

TEST(Version, StringMatchesConstants) {
    EXPECT_STREQ(version_string(), "0.1.0");
}

TEST(Version, StringMacroMatches) {
    EXPECT_STREQ(version_string(), WADJET_VERSION_STRING);
}

}  // namespace wadjet::test
