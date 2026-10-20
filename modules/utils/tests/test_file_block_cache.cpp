#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include "cgl/utils/filesystem.h"

class FileBlockCacheTest : public testing::Test {
 protected:
    std::filesystem::path dir;
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
            ("cgl-cache-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        EXPECT_FALSE(ec) << ec.message();
    }
    auto write(const char* name, const std::string& bytes) {
        auto path = dir / name;
        std::ofstream f(path, std::ios::binary);
        f.write(bytes.data(), bytes.size());
        return path;
    }
};

TEST_F(FileBlockCacheTest, RejectsZeroLimits) {
    EXPECT_THROW(cgl::FileBlockCache(0, 1), std::invalid_argument);
    EXPECT_THROW(cgl::FileBlockCache(1, 0), std::invalid_argument);
}
TEST_F(FileBlockCacheTest, SwitchingFilesAndFailedOpenDiscardOldCache) {
    auto a = write("a", "abcdefgh");
    auto b = write("b", "12345678");
    cgl::FileBlockCache cache(4, 1);
    char out[8]{};
    ASSERT_TRUE(cache.open(a));
    ASSERT_TRUE(cache.read(0, 8, out));
    EXPECT_EQ(std::string(out, 8), "abcdefgh");
    EXPECT_EQ(cache.cachedBlockCount(), 1);
    ASSERT_TRUE(cache.open(b));
    ASSERT_TRUE(cache.read(0, 8, out));
    EXPECT_EQ(std::string(out, 8), "12345678");
    EXPECT_FALSE(cache.open(dir / "missing"));
    EXPECT_FALSE(cache.isStreamReady());
    EXPECT_EQ(cache.fileSize(), 0);
    EXPECT_EQ(cache.cachedBlockCount(), 0);
    EXPECT_FALSE(cache.read(0, 1, out));
    ASSERT_TRUE(cache.open(a));
    EXPECT_FALSE(cache.read(SIZE_MAX, 1, out));
    EXPECT_FALSE(cache.read(0, SIZE_MAX, out));
    EXPECT_FALSE(cache.read(0, 1, nullptr));
}
TEST_F(FileBlockCacheTest, RepeatedCloseAndReopenReleaseBlocksAndFileHandle) {
    auto path = write("data", std::string(128, 'x'));
    cgl::FileBlockCache cache(4, 8);
    char out[128]{};
    for (int i = 0; i < 200; ++i) {
        ASSERT_TRUE(cache.open(path));
        ASSERT_TRUE(cache.read(0, sizeof(out), out));
        EXPECT_EQ(cache.cachedBlockCount(), 8);
        cache.close();
        cache.close();
        EXPECT_EQ(cache.cachedBlockCount(), 0);
        EXPECT_EQ(cache.fileSize(), 0);
        EXPECT_FALSE(cache.isStreamReady());
        // Windows refuses deletion of an open ifstream: this checks handle release.
        ASSERT_TRUE(std::filesystem::remove(path));
        path = write("data", std::string(128, 'x'));
    }
}

TEST_F(FileBlockCacheTest, ShortReadDoesNotThrowOrCachePartialBlock) {
    auto path = write("data", "abcdefgh");
    cgl::FileBlockCache cache(4, 2);
    ASSERT_TRUE(cache.open(path));
    std::filesystem::resize_file(path, 0);
    char out[4]{};
    EXPECT_NO_THROW(EXPECT_FALSE(cache.read(0, 4, out)));
    EXPECT_EQ(cache.cachedBlockCount(), 0);
    cache.close();
    write("data", "12345678");
    ASSERT_TRUE(cache.open(path));
    ASSERT_TRUE(cache.read(4, 4, out));
    EXPECT_EQ(std::string(out, 4), "5678");
}

TEST_F(FileBlockCacheTest, StatisticsAndLruEvictionMatchActualReads) {
    auto path = write("data", "abcdefghijkl");
    cgl::FileBlockCache cache(4, 2);
    ASSERT_TRUE(cache.open(path));
    char out{};
    for (size_t offset : {0, 0, 4, 0, 8, 4}) ASSERT_TRUE(cache.read(offset, 1, &out));
    auto s = cache.cacheStatistics();
    EXPECT_EQ(s.hits, 2); EXPECT_EQ(s.misses, 4);
    EXPECT_EQ(s.fileReadCalls, 4); EXPECT_EQ(s.fileReadBytes, 16);
    EXPECT_EQ(s.evictions, 2); EXPECT_EQ(s.failedReads, 0);
    EXPECT_EQ(s.residentBytes, 8); EXPECT_EQ(s.peakResidentBytes, 8);
    EXPECT_EQ(s.cachedBlocks, 2); EXPECT_EQ(s.budgetBytes, 8);
    cache.trimCache();
    s = cache.cacheStatistics();
    EXPECT_EQ(s.residentBytes, 0); EXPECT_EQ(s.cachedBlocks, 0);
    EXPECT_EQ(s.evictions, 4); EXPECT_EQ(s.peakResidentBytes, 8);
    EXPECT_TRUE(cache.isStreamReady());
    ASSERT_TRUE(cache.read(0, 1, &out)); EXPECT_EQ(out, 'a');
    cache.resetCacheStatistics();
    s = cache.cacheStatistics();
    EXPECT_EQ(s.residentBytes, 4); EXPECT_EQ(s.peakResidentBytes, 4);
    EXPECT_EQ(s.hits, 0); EXPECT_EQ(s.misses, 0); EXPECT_EQ(s.fileReadBytes, 0);
    cache.close();
    EXPECT_EQ(cache.cacheStatistics().residentBytes, 0);
}
TEST_F(FileBlockCacheTest, ShrinkingBudgetPreservesMostRecentAndZeroBypasses) {
    auto path = write("data", "abcdefghijkl");
    cgl::FileBlockCache cache(4, 3);
    ASSERT_TRUE(cache.open(path));
    char out[12]{};
    ASSERT_TRUE(cache.read(0, 12, out));
    ASSERT_TRUE(cache.read(0, 1, out)); // Block 0 becomes most recent.
    cache.setCacheBudget(7);
    EXPECT_EQ(cache.cacheStatistics().residentBytes, 4);
    cache.resetCacheStatistics();
    ASSERT_TRUE(cache.read(0, 1, out));
    EXPECT_EQ(cache.cacheStatistics().hits, 1);
    EXPECT_EQ(cache.cacheStatistics().fileReadCalls, 0);
    cache.setCacheBudget(0);
    EXPECT_EQ(cache.cacheStatistics().residentBytes, 0);
    ASSERT_TRUE(cache.read(0, 12, out));
    EXPECT_EQ(std::string(out, 12), "abcdefghijkl");
    auto s = cache.cacheStatistics();
    EXPECT_EQ(s.bypassReads, 1); EXPECT_EQ(s.fileReadCalls, 1);
    EXPECT_EQ(s.fileReadBytes, 12); EXPECT_EQ(s.misses, 0);
    EXPECT_EQ(s.cachedBlocks, 0);
    cache.setCacheBudget(3); // Positive, but smaller than one block.
    ASSERT_TRUE(cache.read(4, 4, out));
    EXPECT_EQ(cache.cacheStatistics().residentBytes, 0);
    cache.close();
    ASSERT_TRUE(cache.open(path));
    EXPECT_EQ(cache.cacheStatistics().budgetBytes, 3);
    cache.setCacheBudget(4);
    ASSERT_TRUE(cache.read(0, 4, out));
    EXPECT_EQ(cache.cacheStatistics().residentBytes, 4);
}
TEST_F(FileBlockCacheTest, LargeScanAndPartialLastBlockStayWithinBudget) {
    std::string input(4096 * 64 + 3, 'x'); input.back() = 'z';
    auto path = write("data", input);
    cgl::FileBlockCache cache(4096, 3);
    ASSERT_TRUE(cache.open(path));
    std::string output(input.size(), ' ');
    ASSERT_TRUE(cache.read(0, output.size(), output.data()));
    EXPECT_EQ(output, input);
    auto s = cache.cacheStatistics();
    EXPECT_EQ(s.fileReadBytes, input.size()); EXPECT_EQ(s.fileReadCalls, 65);
    EXPECT_EQ(s.residentBytes, 4096 * 3); EXPECT_EQ(s.peakResidentBytes, 4096 * 3);
    EXPECT_EQ(s.evictions, 62);
    for (int i = 0; i < 200; ++i) {
        cache.setCacheBudget(0); cache.trimCache();
        EXPECT_EQ(cache.cacheStatistics().residentBytes, 0);
        cache.setCacheBudget(4096 * 2 + 1);
        ASSERT_TRUE(cache.read(0, output.size(), output.data()));
        EXPECT_EQ(cache.cacheStatistics().residentBytes, 4096 * 2);
    }
}
TEST_F(FileBlockCacheTest, FailedIoCountersAndOverflowingBudget) {
    EXPECT_THROW(cgl::FileBlockCache(SIZE_MAX, 2), std::invalid_argument);
    auto path = write("data", "abcdefgh");
    cgl::FileBlockCache cache(4, 2);
    ASSERT_TRUE(cache.open(path));
    std::filesystem::resize_file(path, 0);
    char out[4]{};
    EXPECT_FALSE(cache.read(0, 4, out));
    auto s = cache.cacheStatistics();
    EXPECT_EQ(s.failedReads, 1); EXPECT_EQ(s.fileReadCalls, 1);
    EXPECT_EQ(s.fileReadBytes, 0); EXPECT_EQ(s.residentBytes, 0);
    cache.resetCacheStatistics(); cache.setCacheBudget(0);
    EXPECT_FALSE(cache.read(0, 4, out));
    s = cache.cacheStatistics();
    EXPECT_EQ(s.failedReads, 1); EXPECT_EQ(s.bypassReads, 1);
    EXPECT_EQ(s.residentBytes, 0);
}
