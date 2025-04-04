// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <memory>
#include "cgl/settings/settings_loader.h"

TEST(SettingsLoader, LoadingTest) {
    cgl::SettingsLoader loader;
    EXPECT_EQ(loader.settings(), nullptr);

    // Verify the result
    cgl::Results result = loader.load();
    EXPECT_EQ(result, cgl::Results::Success);
    EXPECT_NE(loader.settings(), nullptr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
