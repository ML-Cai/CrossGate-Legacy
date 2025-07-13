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
#include "utils/file_utils.h"

// -----------------------------------------------------------------------------
namespace {

constexpr size_t DATA_COUNT = 2000;
using DATA_TYPE = size_t;

class FileUtilsTest : public testing::Test {
 protected:
    std::filesystem::path testFilePath;

    void SetUp() override {
        // export test file
        testFilePath = std::filesystem::temp_directory_path() / "test.bin";
        std::ofstream out(testFilePath, std::ios::binary);
        for (size_t i = 0 ; i < DATA_COUNT ; i++) {
            DATA_TYPE data = i;
            out.write(reinterpret_cast<const char*>(&data), sizeof(data));
        }
    }
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(FileUtilsTest, CanReadFile) {
    auto openInfo = cgl::TryOpenBinaryFile(testFilePath);
    ASSERT_TRUE(openInfo.stream.has_value());
    ASSERT_TRUE(openInfo.stream->is_open());
    ASSERT_TRUE(cgl::IsFileOpen(openInfo));

    auto size = cgl::GetFileSize(&openInfo);
    ASSERT_EQ(size, DATA_COUNT * sizeof(DATA_TYPE));
}

// -----------------------------------------------------------------------------
TEST_F(FileUtilsTest, FileBlockCacheTest) {
    cgl::FileBlockCache cache;

    // open cache test
    ASSERT_FALSE(cache.isStreamReady());
    ASSERT_EQ(cache.open(testFilePath), cgl::Results::Success);
    ASSERT_TRUE(cache.isStreamReady());

    // verify data
    constexpr size_t DATA_LENGTH = DATA_COUNT * sizeof(DATA_TYPE);
    uint8_t data[DATA_LENGTH];
    size_t *pReadedData = reinterpret_cast<size_t *>(data);
    ASSERT_EQ(cache.read(0, DATA_LENGTH, data), cgl::Results::Success);

    for (size_t i = 0 ; i < DATA_COUNT ; i++) {
        ASSERT_EQ(pReadedData[i], i);
    }


    // verify file length
    ASSERT_EQ(cache.fileSize(), DATA_LENGTH);

    // verify data with offset
    std::vector<size_t> offsetTestIdxList = {1, 511, 1023, 1025};
    for (const auto& offsetTestIdx : offsetTestIdxList) {
        size_t offset = offsetTestIdx * sizeof(DATA_TYPE);
        size_t len = (DATA_COUNT - offsetTestIdx) * sizeof(DATA_TYPE);

        ASSERT_EQ(cache.read(offset, len, data), cgl::Results::Success);

        for (size_t i = offsetTestIdx ; i < DATA_COUNT ; i++) {
            ASSERT_EQ(pReadedData[i - offsetTestIdx], i);
        }
    }

    // out of range test
    ASSERT_EQ(cache.read(0, DATA_LENGTH + 1, data), cgl::Results::InvalidArgs);
    ASSERT_EQ(cache.read(1, DATA_LENGTH, data), cgl::Results::InvalidArgs);
    ASSERT_EQ(cache.read(0, DATA_LENGTH, nullptr), cgl::Results::InvalidArgs);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
