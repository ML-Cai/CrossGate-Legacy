// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include "cgl/common/formatters.h"

// -----------------------------------------------------------------------------
TEST(common_enum_test, ResultsToStr) {
    auto str = cgl::ToStr(cgl::Results::Success);
    EXPECT_EQ(str, "Success");
}

// -----------------------------------------------------------------------------
TEST(common_enum_test, ResultsToStr_OutOfRangeValue) {
    auto invalid = static_cast<cgl::Results>(255);
    EXPECT_EQ(cgl::ToStr(invalid), "Unknown");
}

// -----------------------------------------------------------------------------
TEST(common_enum_test, CrossGateVersionToStr) {
    auto str = cgl::ToStr(cgl::CrossGateVersion::CG_VERSION_Classic);
    EXPECT_EQ(str, "CG_VERSION_Classic");
}

// -----------------------------------------------------------------------------
TEST(common_enum_test, CrossGateVersionToStr_OutOfRangeValue) {
    auto invalid = static_cast<cgl::CrossGateVersion>(255);
    EXPECT_EQ(cgl::ToStr(invalid), "Unknown");
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
