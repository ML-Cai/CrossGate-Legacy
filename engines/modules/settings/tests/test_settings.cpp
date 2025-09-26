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
#include <fstream>
#include <string>
#include "cgl/settings/settings.h"

// -----------------------------------------------------------------------------
TEST(settings_test, LoadingTest) {
    // Verify the result
    EXPECT_EQ(cgl::StaticSettings::General::AssetsRelPath, "bin");
}

TEST(settings_test, LoadInvalidFile) {
    auto settings = cgl::LoadSettings("non_existent_file.ini");
    EXPECT_EQ(settings, nullptr);
}

TEST(settings_test, LoadValidFile) {
    // Create a temporary INI file for testing
    const std::string test_ini_path = "test_valid.ini";
    std::ofstream ini_file(test_ini_path);
    ini_file << "[General]\n";
    ini_file << "EngineRootPath=C:/Games/CrossGate\n";
    ini_file << "\n[Display]\n";
    ini_file << "ViewportScale=1.5\n";
    ini_file.close();

    auto settings = cgl::LoadSettings(test_ini_path);
    ASSERT_NE(settings, nullptr);
    EXPECT_EQ(settings->general.EngineRootPath, "C:/Games/CrossGate");
    EXPECT_FLOAT_EQ(settings->display.ViewportScale, 1.5f);

    // Clean up the temporary file
    std::filesystem::remove(test_ini_path);
}

TEST(settings_test, LoadPartialFile) {
    // Create a temporary INI file with only one section
    const std::string test_ini_path = "test_partial.ini";
    std::ofstream ini_file(test_ini_path);
    ini_file << "[General]\n";
    ini_file << "EngineRootPath=C:/Games/CrossGate\n";
    ini_file.close();

    auto settings = cgl::LoadSettings(test_ini_path);
    ASSERT_NE(settings, nullptr);
    EXPECT_EQ(settings->general.EngineRootPath, "C:/Games/CrossGate");
    // Default value for ViewportScale should be 1.0f
    EXPECT_FLOAT_EQ(settings->display.ViewportScale, 1.0f);

    // Clean up the temporary file
    std::filesystem::remove(test_ini_path);
}

TEST(settings_test, LoadInvalidFormatFile) {
    // Create a temporary INI file with invalid format
    const std::string test_ini_path = "test_invalid_format.ini";
    std::ofstream ini_file(test_ini_path);
    ini_file << "This is not a valid INI format\n";
    ini_file.close();

    auto default_settings = cgl::LoadSettings();
    auto settings = cgl::LoadSettings(test_ini_path);
    ASSERT_NE(settings, nullptr);

    // Default values should be set
    EXPECT_EQ(*settings, *default_settings);

    // Clean up the temporary file
    std::filesystem::remove(test_ini_path);
}

TEST(settings_test, LoadEmptyFile) {
    // Create an empty INI file
    const std::string test_ini_path = "test_empty.ini";
    std::ofstream ini_file(test_ini_path);
    ini_file.close();

    auto default_settings = cgl::LoadSettings();
    auto settings = cgl::LoadSettings(test_ini_path);
    ASSERT_NE(settings, nullptr);

    // Default values should be set
    EXPECT_EQ(*settings, *default_settings);

    // Clean up the temporary file
    std::filesystem::remove(test_ini_path);
}

TEST(settings_test, LoadFileWithExtraWhitespace) {
    // Create a temporary INI file with extra whitespace
    const std::string test_ini_path = "test_whitespace.ini";
    std::ofstream ini_file(test_ini_path);
    ini_file << "   [General]   \n";
    ini_file << "   EngineRootPath = C:/Games/CrossGate   \n";
    ini_file << "\n   [Display]   \n";
    ini_file << "   ViewportScale = 2.0   \n";
    ini_file.close();

    auto settings = cgl::LoadSettings(test_ini_path);
    ASSERT_NE(settings, nullptr);
    EXPECT_EQ(settings->general.EngineRootPath, "C:/Games/CrossGate");
    EXPECT_FLOAT_EQ(settings->display.ViewportScale, 2.0f);

    // Clean up the temporary file
    std::filesystem::remove(test_ini_path);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
