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
#include "cgl/utils/filesystem.h"

// -----------------------------------------------------------------------------
TEST(utils_filesystem_test, LoadInvalidFile) {
    auto fileInfo = cgl::TryOpenBinaryFile("not_exist_binary.txt");
    EXPECT_EQ(fileInfo.stream, std::nullopt);
}

// -----------------------------------------------------------------------------
TEST(utils_filesystem_test, LoadValidFile) {
    std::string file_name = "_test_file_valid.txt";
    const std::filesystem::path file_path = file_name;

    // write dummy file
    std::ofstream outFile(file_name);
    outFile << "; This is a comment line\n";
    outFile.close();

    auto fileInfo = cgl::TryOpenBinaryFile(file_name);
    EXPECT_NE(fileInfo.stream, std::nullopt);
    EXPECT_EQ(fileInfo.path, file_path);
}


// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
