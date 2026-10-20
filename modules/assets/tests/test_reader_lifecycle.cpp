#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "cgl/assets/anime_info_reader.h"
#include "cgl/assets/graphics_info_reader.h"
#include <vector>
#include "cgl/assets/anime_data_reader.h"
#include "cgl/assets/graphics_data_reader.h"
#include "cgl/settings/settings.h"

class ResourceReaderTest : public testing::Test {
 protected:
    std::filesystem::path dir;
    cgl::Settings settings{};
    static constexpr auto version = cgl::CrossGateVersion::CG_VERSION_Classic;
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
            ("cgl-reader-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir / "bin");
        settings.general.EngineRootPath = dir.string();
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        EXPECT_FALSE(ec) << ec.message();
    }
    void write(const char* name, const std::vector<uint8_t>& bytes) {
        std::ofstream f(dir / "bin" / name, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    static void word(std::vector<uint8_t>& bytes, uint32_t value, int width) {
        for (int i = 0; i < width; ++i) bytes.push_back(uint8_t(value >> (i * 8)));
    }
    static void motion(std::vector<uint8_t>& bytes, uint16_t direction, uint32_t count) {
        word(bytes, direction, 2); word(bytes, 0, 2);
        word(bytes, 100, 4); word(bytes, count, 4);
        for (uint32_t i = 0; i < count; ++i) {
            word(bytes, i + 123, 4);
            for (int k = 0; k < 6; ++k) bytes.push_back(0);
        }
    }
    static auto info(uint16_t count = 1) {
        cgl::AnimeResourceInfo result{};
        result.serialNum = {version, 100000};
        result.motionCount = count;
        return result;
    }
};
TEST_F(ResourceReaderTest, AnimationLargeMotionCloseAndCallerOwnership) {
    std::vector<uint8_t> bytes; motion(bytes, 0, 1025);
    write("Anime_4.bin", bytes);
    auto reader = cgl::IAnimeDataReader::create({&settings, version});
    cgl::AnimeResourceData out{};
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        ASSERT_EQ(reader->query(info(), &out), cgl::Results::Success);
        ASSERT_EQ(out.motionMap.size(), 1);
        const auto& desc = out.motionMap.begin()->second;
        EXPECT_EQ(desc.version, version);
        ASSERT_EQ(desc.motionGraphicsSerialNums.size(), 1025);
        EXPECT_EQ(desc.motionGraphicsSerialNums.back(), 1147);
        reader->close(); reader->close();
        EXPECT_NE(reader->query(info(), &out), cgl::Results::Success);
        EXPECT_EQ(out.motionMap.begin()->second.motionGraphicsSerialNums.back(), 1147);
        ASSERT_TRUE(std::filesystem::remove(dir / "bin/Anime_4.bin"));
        write("Anime_4.bin", bytes);
    }
}
TEST_F(ResourceReaderTest, TruncatedAnimationDoesNotPublishPartialOutput) {
    std::vector<uint8_t> bytes; motion(bytes, 0, 3); motion(bytes, 1, 2);
    auto reader = cgl::IAnimeDataReader::create({&settings, version});
    cgl::AnimeResourceData out{};
    out.serialNum = {version, 777};
    for (size_t len = 0; len < bytes.size(); ++len) {
        write("Anime_4.bin", {bytes.begin(), bytes.begin() + len});
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        EXPECT_NE(reader->query(info(2), &out), cgl::Results::Success) << len;
        EXPECT_EQ(out.serialNum.value, 777);
        EXPECT_TRUE(out.motionMap.empty());
        reader->close();
    }
}
TEST_F(ResourceReaderTest, UnknownMotionSkipsPayloadAndHugeCountIsRejected) {
    std::vector<uint8_t> bytes; motion(bytes, 65535, 2); motion(bytes, 0, 1);
    write("Anime_4.bin", bytes);
    auto reader = cgl::IAnimeDataReader::create({&settings, version});
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    cgl::AnimeResourceData out{};
    ASSERT_EQ(reader->query(info(2), &out), cgl::Results::Success);
    ASSERT_EQ(out.motionMap.size(), 1);
    EXPECT_EQ(out.motionMap.begin()->second.motionGraphicsSerialNums.front(), 123);
    reader->close();
    bytes.clear(); motion(bytes, 0, 0);
    for (int i = 8; i < 12; ++i) bytes[i] = 255;
    write("Anime_4.bin", bytes);
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    EXPECT_NE(reader->query(info(), &out), cgl::Results::Success);
    EXPECT_EQ(out.motionMap.size(), 1);
}
TEST_F(ResourceReaderTest, InvalidGraphicsLoadCanRetryAndClosePreservesCallerPixels) {
    write("Graphic_66.bin", {'X','X',0});
    auto reader = cgl::IGraphicsDataReader::create({&settings, version});
    EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
    EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/Graphic_66.bin"));
    std::vector<uint8_t> bytes{'R','D',0,0};
    word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, 17, 4); bytes.push_back(42);
    write("Graphic_66.bin", bytes);
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{};
    ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
    reader->close();
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/Graphic_66.bin"));
    ASSERT_NE(out.pData, nullptr);
    EXPECT_EQ(out.pData[0], 42);
}

TEST_F(ResourceReaderTest, AnimationIndexReloadDropsOldEntriesAndClosesFile) {
    std::vector<uint8_t> bytes;
    word(bytes, 100, 4); word(bytes, 0, 4); word(bytes, 1, 2); word(bytes, 0, 2);
    write("AnimeInfo_4.bin", bytes);
    auto reader = cgl::IAnimeInfoReader::create({&settings, version});
    cgl::AnimeResourceInfo out{};
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        ASSERT_EQ(reader->query({version, 100}, &out), cgl::Results::Success);
        ASSERT_TRUE(std::filesystem::remove(dir / "bin/AnimeInfo_4.bin"));
        write("AnimeInfo_4.bin", {1});
        EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
        EXPECT_NE(reader->query({version, 100}, &out), cgl::Results::Success);
        reader->close(); reader->close();
        write("AnimeInfo_4.bin", bytes);
    }
}
TEST_F(ResourceReaderTest, GraphicsIndexCloseInvalidatesPointersAndAllowsRetry) {
    std::vector<uint8_t> bytes(40, 0);
    bytes[0] = 100; bytes[28] = 2; bytes[29] = 3;
    write("GraphicInfo_66.bin", bytes);
    auto reader = cgl::IGraphicsInfoReader::create({&settings, version});
    cgl::GraphicsResourceSerialNum key{cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum, version, 100};
    cgl::GraphicsResourceInfo out{};
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        ASSERT_EQ(reader->query(key, &out), cgl::Results::Success);
        EXPECT_EQ(out.tileX, 2); EXPECT_EQ(out.tileY, 3);
        ASSERT_TRUE(std::filesystem::remove(dir / "bin/GraphicInfo_66.bin"));
        reader->close(); reader->close();
        EXPECT_EQ(reader->infoCount(), 0);
        EXPECT_NE(reader->query(key, &out), cgl::Results::Success);
        write("GraphicInfo_66.bin", {1});
        EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
        EXPECT_EQ(reader->infoCount(), 0);
        write("GraphicInfo_66.bin", bytes);
    }
}

TEST_F(ResourceReaderTest, GraphicsRejectsInvalidAllocationSizesWithoutChangingOutput) {
    auto reader = cgl::IGraphicsDataReader::create({&settings, version});
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{}; out.width = 777;
    for (auto width : {0u, 0xffffffffu, 0x7fffffffu}) {
        std::vector<uint8_t> bytes{'R','D',0,0};
        word(bytes, width, 4); word(bytes, 0x7fffffff, 4); word(bytes, 16, 4);
        write("Graphic_66.bin", bytes);
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
        EXPECT_EQ(out.width, 777); EXPECT_EQ(out.pData, nullptr);
        reader->close();
    }
}
TEST_F(ResourceReaderTest, GraphicsRejectsInvalidRecordLengthsAndOffsets) {
    auto reader = cgl::IGraphicsDataReader::create({&settings, version});
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{}; out.width = 777;
    for (auto size : {0u, 15u, 0xffffffffu}) {
        std::vector<uint8_t> bytes{'R','D',1,0};
        word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, size, 4);
        bytes.push_back(0xc1);
        write("Graphic_66.bin", bytes);
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
        resource.dataOffset = UINT32_MAX;
        EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
        resource.dataOffset = 0;
        EXPECT_EQ(out.width, 777); EXPECT_EQ(out.pData, nullptr);
        reader->close();
    }
}
TEST_F(ResourceReaderTest, Puk2PaletteReadsExactlyOneRecord) {
    auto reader = cgl::IGraphicsDataReader::create({&settings, version});
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    for (bool compressed : {false, true}) {
        std::vector<uint8_t> bytes{'R','D',uint8_t(compressed ? 3 : 2),0};
        word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, compressed ? 25 : 24, 4);
        word(bytes, 3, 4);
        if (compressed) bytes.push_back(4); // Raw run of 4 bytes.
        bytes.insert(bytes.end(), {42, 10, 20, 30});
        write("Graphic_66.bin", bytes);
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        cgl::GraphicsResourceData out{};
        ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
        reader->close();
        EXPECT_EQ(out.pData[0], 42);
        ASSERT_EQ(out.pPaletteData, out.pData.get() + 1);
        EXPECT_EQ(out.pPaletteData[0], 10); EXPECT_EQ(out.pPaletteData[2], 30);
    }
}

TEST_F(ResourceReaderTest, AnimationCacheBudgetTrimAndHotReads) {
    std::vector<uint8_t> bytes; motion(bytes, 0, 3);
    write("Anime_4.bin", bytes);
    auto reader = cgl::IAnimeDataReader::create({&settings, version, 4096});
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    cgl::AnimeResourceData out{};
    ASSERT_EQ(reader->query(info(), &out), cgl::Results::Success);
    auto cold = reader->cacheStatistics();
    EXPECT_EQ(cold.budgetBytes, 4096); EXPECT_EQ(cold.residentBytes, 4096);
    EXPECT_EQ(cold.fileReadCalls, 1); EXPECT_EQ(cold.fileReadBytes, bytes.size());
    ASSERT_EQ(reader->query(info(), &out), cgl::Results::Success);
    auto hot = reader->cacheStatistics();
    EXPECT_EQ(hot.fileReadCalls, cold.fileReadCalls); EXPECT_GT(hot.hits, cold.hits);
    reader->trimCache();
    EXPECT_EQ(reader->cacheStatistics().residentBytes, 0);
    ASSERT_EQ(out.motionMap.begin()->second.motionGraphicsSerialNums.front(), 123);
    reader->setCacheBudget(0); reader->resetCacheStatistics();
    ASSERT_EQ(reader->query(info(), &out), cgl::Results::Success);
    auto uncached = reader->cacheStatistics();
    EXPECT_EQ(uncached.residentBytes, 0); EXPECT_EQ(uncached.fileReadCalls, 2);
    EXPECT_EQ(uncached.bypassReads, 2); EXPECT_EQ(uncached.scratchBytes, 0);
}
TEST_F(ResourceReaderTest, GraphicsTrimReleasesScratchAndCacheButPreservesOutput) {
    std::vector<uint8_t> bytes{'R','D',0,0};
    word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, 17, 4); bytes.push_back(42);
    write("Graphic_66.bin", bytes);
    auto reader = cgl::IGraphicsDataReader::create({&settings, version, 0});
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{};
    ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
    auto s = reader->cacheStatistics();
    EXPECT_EQ(s.residentBytes, 0); EXPECT_GE(s.scratchBytes, 1);
    reader->trimCache();
    EXPECT_EQ(reader->cacheStatistics().scratchBytes, 0);
    ASSERT_EQ(out.pData[0], 42);
    reader->setCacheBudget(4096); reader->resetCacheStatistics();
    ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
    EXPECT_EQ(reader->cacheStatistics().residentBytes, 4096);
    reader->trimCache(); reader->trimCache();
    EXPECT_EQ(reader->cacheStatistics().residentBytes, 0);
    EXPECT_EQ(reader->cacheStatistics().scratchBytes, 0);
    ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
    reader->close();
    EXPECT_EQ(reader->cacheStatistics().residentBytes, 0);
    EXPECT_EQ(reader->cacheStatistics().scratchBytes, 0);
}
