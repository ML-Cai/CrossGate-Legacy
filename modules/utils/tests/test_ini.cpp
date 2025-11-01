// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include "cgl/utils/ini.h"

TEST(utils_ini_test, LoadValidIniFile) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_valid.ini");
    outFile << "; This is a comment line\n";
    outFile << "[Section1]\n";
    outFile << "Key1=Value1\n";
    outFile << "Key2=Value2\n";
    outFile << "\n";
    outFile << "[Section2]\n";
    outFile << "KeyA=ValueA\n";
    outFile << "KeyB=ValueB\n";
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_valid.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_EQ(iniData["Section1"]["Key1"], "Value1");
    EXPECT_EQ(iniData["Section1"]["Key2"], "Value2");
    EXPECT_EQ(iniData["Section2"]["KeyA"], "ValueA");
    EXPECT_EQ(iniData["Section2"]["KeyB"], "ValueB");

    // clean up
    std::remove("_test_file_valid.ini");
}

TEST(utils_ini_test, LoadInvalidIniFile) {
    cgl::INI iniData;

    bool result = cgl::LoadIniFile("non_existent_file.ini", &iniData);
    EXPECT_FALSE(result);
    EXPECT_TRUE(iniData.empty());
}

TEST(utils_ini_test, LoadIniFileWithCommentsAndWhitespace) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_comments.ini");
    outFile << "   ; This is a comment line with leading whitespace\n";
    outFile << "[Section1]   \n";
    outFile << "   Key1 = Value1   \n";
    outFile << "Key2=   Value2\n";
    outFile << "\n";
    outFile << "   [Section2]   \n";
    outFile << "KeyA=ValueA   \n";
    outFile << "   KeyB =ValueB\n";
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_comments.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_EQ(iniData["Section1"]["Key1"], "Value1");
    EXPECT_EQ(iniData["Section1"]["Key2"], "Value2");
    EXPECT_EQ(iniData["Section2"]["KeyA"], "ValueA");
    EXPECT_EQ(iniData["Section2"]["KeyB"], "ValueB");

    // clean up
    std::remove("_test_file_comments.ini");
}

TEST(utils_ini_test, LoadIniFileWithMissingSections) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_no_sections.ini");
    outFile << "Key1=Value1\n";
    outFile << "Key2=Value2\n";
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_no_sections.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_EQ(iniData[""]["Key1"], "Value1");
    EXPECT_EQ(iniData[""]["Key2"], "Value2");

    // clean up
    std::remove("_test_file_no_sections.ini");
}

TEST(utils_ini_test, LoadIniFileWithDuplicateKeys) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_duplicate_keys.ini");
    outFile << "[Section1]\n";
    outFile << "Key1=Value1\n";
    outFile << "Key1=Value2\n";  // duplicate key
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_duplicate_keys.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_EQ(iniData["Section1"]["Key1"], "Value2");  // last value should be kept

    // clean up
    std::remove("_test_file_duplicate_keys.ini");
}

TEST(utils_ini_test, LoadIniFileWithNoKeyValuePairs) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_no_key_values.ini");
    outFile << "[Section1]\n";
    outFile << "\n";
    outFile << "[Section2]\n";
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_no_key_values.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_TRUE(iniData["Section1"].empty());
    EXPECT_TRUE(iniData["Section2"].empty());

    // clean up
    std::remove("_test_file_no_key_values.ini");
}

TEST(utils_ini_test, LoadEmptyIniFile) {
    cgl::INI iniData;

    // write empty ini file
    std::ofstream outFile("_test_file_empty.ini");
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_empty.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_TRUE(iniData.empty());

    // clean up
    std::remove("_test_file_empty.ini");
}

TEST(utils_ini_test, LoadIniFileWithMalformedLines) {
    cgl::INI iniData;

    // write dummy ini file
    std::ofstream outFile("_test_file_malformed.ini");
    outFile << "[Section1]\n";
    outFile << "Key1=Value1\n";
    outFile << "MalformedLineWithoutEqualSign\n";  // malformed line
    outFile << "Key2=Value2\n";
    outFile.close();

    bool result = cgl::LoadIniFile("_test_file_malformed.ini", &iniData);
    EXPECT_TRUE(result);
    EXPECT_EQ(iniData["Section1"]["Key1"], "Value1");
    EXPECT_EQ(iniData["Section1"]["Key2"], "Value2");
    EXPECT_EQ(iniData["Section1"].size(), 2);  // only valid key-value pairs should be counted

    // clean up
    std::remove("_test_file_malformed.ini");
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
