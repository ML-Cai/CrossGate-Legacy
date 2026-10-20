#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "cgl/assets/anime_info_reader.h"
#include "cgl/assets/palette_reader.h"
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
        bytes.insert(bytes.end(), {0, 10, 20, 30});
        write("Graphic_66.bin", bytes);
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        cgl::GraphicsResourceData out{};
        ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
        reader->close();
        EXPECT_EQ(out.pData[0], 0);
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
    EXPECT_EQ(s.residentBytes, 0); EXPECT_EQ(s.scratchBytes, 0);
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

TEST_F(ResourceReaderTest, LargeCompressedScratchIsReleasedAndSmallScratchIsBounded) {
    auto reader = cgl::IGraphicsDataReader::create({&settings, version, 0});
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{};
    std::vector<uint8_t> file;
    std::vector<uint32_t> offsets;
    const std::vector<uint32_t> sizes{16, 1024 * 1024, 32};
    for (uint32_t pixels : sizes) {
        std::vector<uint8_t> payload;
        for (uint32_t done = 0; done < pixels;) {
            uint32_t count = std::min(65535u, pixels - done);
            payload.push_back(0x20); // Large raw run: 0x20, high byte, low byte.
            payload.push_back(uint8_t(count >> 8)); payload.push_back(uint8_t(count));
            payload.insert(payload.end(), count, 7); done += count;
        }
        offsets.push_back(uint32_t(file.size()));
        file.insert(file.end(), {'R','D',1,0});
        word(file, pixels, 4); word(file, 1, 4); word(file, uint32_t(16 + payload.size()), 4);
        file.insert(file.end(), payload.begin(), payload.end());
    }
    write("Graphic_66.bin", file);
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    for (int pass = 0; pass < 6; ++pass) {
        for (size_t i = 0; i < offsets.size(); ++i) {
            resource.dataOffset = offsets[i];
            ASSERT_EQ(reader->query(resource, &out), cgl::Results::Success);
            EXPECT_EQ(out.pData[0], 7); EXPECT_EQ(out.pData[sizes[i] - 1], 7);
            auto stats = reader->cacheStatistics();
            EXPECT_EQ(stats.residentBytes, 0);
            if (sizes[i] > 32) EXPECT_EQ(stats.scratchBytes, 0);
            else { EXPECT_GT(stats.scratchBytes, 0); EXPECT_LE(stats.scratchBytes, 1024 * 1024); }
        }
    }
    reader->close();
    EXPECT_EQ(reader->cacheStatistics().scratchBytes, 0);
}
TEST_F(ResourceReaderTest, LargeFailedDecodeDoesNotRetainScratchOrReplaceOutput) {
    const uint32_t payloadSize = 1024 * 1024 + 1;
    std::vector<uint8_t> bytes{'R','D',1,0};
    word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, 16 + payloadSize, 4);
    bytes.resize(16 + payloadSize, 0x40); // Unknown opcode.
    write("Graphic_66.bin", bytes);
    auto reader = cgl::IGraphicsDataReader::create({&settings, version, 0});
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    cgl::GraphicsResourceInfo resource{}; resource.version = version;
    cgl::GraphicsResourceData out{}; out.width = 777;
    EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
    EXPECT_EQ(out.width, 777); EXPECT_EQ(out.pData, nullptr);
    EXPECT_EQ(reader->cacheStatistics().scratchBytes, 0);
}
TEST_F(ResourceReaderTest, PrivatePaletteRejectsUnaddressableColors) {
    for (auto paletteSize : {2u, 3u, 769u}) {
        std::vector<uint8_t> bytes{'R','D',2,0};
        word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, 21 + paletteSize, 4);
        word(bytes, paletteSize, 4); bytes.push_back(42);
        bytes.resize(21 + paletteSize, 0);
        write("Graphic_66.bin", bytes);
        auto reader = cgl::IGraphicsDataReader::create({&settings, version, 0});
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        cgl::GraphicsResourceInfo resource{}; resource.version = version;
        cgl::GraphicsResourceData out{}; out.width = 777;
        EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
        EXPECT_EQ(out.width, 777); EXPECT_EQ(out.pData, nullptr);
    }
}
TEST_F(ResourceReaderTest, PaletteCacheCopiesValuesClosesFilesAndSupportsInvalidation) {
    std::filesystem::create_directories(dir / "bin/pal");
    auto reader = cgl::IPaletteReader::create({&settings});
    cgl::PaletteData256 out{};
    write("pal/palet_00.cgp", std::vector<uint8_t>(708, 7));
    ASSERT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 7);
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/pal/palet_00.cgp"));
    out[16].BGR.B = 99;
    ASSERT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 7); // Cached copy, no open input or alias into caller output.
    reader->clearCache(); reader->clearCache();
    EXPECT_NE(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 7);
    write("pal/palet_00.cgp", std::vector<uint8_t>(708, 8));
    ASSERT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 8);
    EXPECT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, nullptr), cgl::Results::InvalidArgs);
    EXPECT_EQ(reader->read(static_cast<cgl::EnvironmentPaletteTypes>(255), &out), cgl::Results::InvalidArgs);
}
TEST_F(ResourceReaderTest, PaletteFailuresAreNotCachedAndDoNotChangeOutput) {
    std::filesystem::create_directories(dir / "bin/pal");
    auto reader = cgl::IPaletteReader::create({&settings});
    cgl::PaletteData256 out{}; out[16].BGR.B = 77;
    for (size_t size : {0u, 1u, 707u, 709u}) {
        write("pal/palet_00.cgp", std::vector<uint8_t>(size, 9));
        EXPECT_NE(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
        EXPECT_EQ(out[16].BGR.B, 77);
    }
    write("pal/palet_00.cgp", std::vector<uint8_t>(708, 9));
    ASSERT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 9);
    settings.general.EngineRootPath = (dir / "missing-root").string();
    EXPECT_NE(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 9);
    settings.general.EngineRootPath = dir.string();
    write("pal/palet_00.cgp", std::vector<uint8_t>(708, 10));
    ASSERT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &out), cgl::Results::Success);
    EXPECT_EQ(out[16].BGR.B, 10);
}

TEST_F(ResourceReaderTest, TruncatedRleOperandsDoNotPublishPartialOutput) {
    const std::vector<std::vector<uint8_t>> payloads{
        {0x90}, {0xa0, 0x01}, {0x10}, {0x20, 0x01}, {0x80}, {0xd0}, {0xe0, 0x01}
    };
    for (const auto& payload : payloads) {
        std::vector<uint8_t> bytes{'R','D',1,0};
        word(bytes, 1, 4); word(bytes, 1, 4); word(bytes, uint32_t(16 + payload.size()), 4);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        write("Graphic_66.bin", bytes);
        auto reader = cgl::IGraphicsDataReader::create({&settings, version, 0});
        ASSERT_EQ(reader->load(), cgl::Results::Success);
        cgl::GraphicsResourceInfo resource{}; resource.version = version;
        cgl::GraphicsResourceData out{}; out.width = 777;
        EXPECT_NE(reader->query(resource, &out), cgl::Results::Success);
        EXPECT_EQ(out.width, 777); EXPECT_EQ(out.pData, nullptr);
    }
}

TEST_F(ResourceReaderTest, GraphicsIndexLimitsAreInclusiveAndFailureCanRetry) {
    std::vector<uint8_t> bytes(80, 0); bytes[0] = 1; bytes[40] = 2;
    for (auto limits : {std::pair<size_t,size_t>{80, 3}, {120, 2}}) {
        auto reader = cgl::IGraphicsInfoReader::create({&settings, version, limits.first, limits.second});
        for (int i = 0; i < 50; ++i) {
            write("GraphicInfo_66.bin", bytes);
            ASSERT_EQ(reader->load(), cgl::Results::Success);
            EXPECT_EQ(reader->infoCount(), 2);
            reader->close();
            auto oversized = bytes; oversized.resize(120, 0); oversized[80] = 3;
            write("GraphicInfo_66.bin", oversized);
            EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
            EXPECT_EQ(reader->infoCount(), 0);
            cgl::GraphicsResourceInfo out{}; out.width = 777;
            EXPECT_NE(reader->query({cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum, version, 1}, &out),
                      cgl::Results::Success);
            EXPECT_EQ(out.width, 777);
            ASSERT_TRUE(std::filesystem::remove(dir / "bin/GraphicInfo_66.bin"));
        }
    }
}
TEST_F(ResourceReaderTest, AnimeIndexLimitsAreInclusiveAndReloadDropsPreviousEntries) {
    std::vector<uint8_t> bytes;
    word(bytes, 1, 4); word(bytes, 11, 4); word(bytes, 1, 2); word(bytes, 0, 2);
    word(bytes, 2, 4); word(bytes, 22, 4); word(bytes, 1, 2); word(bytes, 0, 2);
    for (auto limits : {std::pair<size_t,size_t>{24, 3}, {36, 2}}) {
        auto reader = cgl::IAnimeInfoReader::create({&settings, version, limits.first, limits.second});
        for (int i = 0; i < 50; ++i) {
            write("AnimeInfo_4.bin", bytes);
            ASSERT_EQ(reader->load(), cgl::Results::Success);
            cgl::AnimeResourceInfo out{};
            ASSERT_EQ(reader->query({version, 1}, &out), cgl::Results::Success);
            EXPECT_EQ(out.dataOffset, 11);
            auto oversized = bytes; oversized.resize(36, 0);
            write("AnimeInfo_4.bin", oversized);
            EXPECT_EQ(reader->load(), cgl::Results::InvalidFile);
            out.dataOffset = 777;
            EXPECT_NE(reader->query({version, 1}, &out), cgl::Results::Success);
            EXPECT_EQ(out.dataOffset, 777);
            ASSERT_TRUE(std::filesystem::remove(dir / "bin/AnimeInfo_4.bin"));
        }
    }
}
TEST_F(ResourceReaderTest, ZeroIndexLimitsDisablePreloading) {
    for (auto limits : {std::pair<size_t,size_t>{0, 2}, {80, 0}}) {
        write("GraphicInfo_66.bin", std::vector<uint8_t>(40, 0));
        auto graphics = cgl::IGraphicsInfoReader::create({&settings, version, limits.first, limits.second});
        EXPECT_EQ(graphics->load(), cgl::Results::InvalidFile);
        EXPECT_EQ(graphics->infoCount(), 0);
        ASSERT_TRUE(std::filesystem::remove(dir / "bin/GraphicInfo_66.bin"));
        write("AnimeInfo_4.bin", std::vector<uint8_t>(12, 0));
        auto anime = cgl::IAnimeInfoReader::create({&settings, version, limits.first, limits.second});
        EXPECT_EQ(anime->load(), cgl::Results::InvalidFile);
        ASSERT_TRUE(std::filesystem::remove(dir / "bin/AnimeInfo_4.bin"));
    }
}
TEST_F(ResourceReaderTest, AnimeIndexBatchesPreserveLastDuplicateAndAllEntries) {
    std::vector<uint8_t> bytes;
    for (uint32_t i = 0; i < 1025; ++i) {
        word(bytes, 100 + i, 4); word(bytes, i * 10, 4); word(bytes, 2, 2); word(bytes, 0, 2);
    }
    word(bytes, 100, 4); word(bytes, 9999, 4); word(bytes, 3, 2); word(bytes, 0, 2);
    write("AnimeInfo_4.bin", bytes);
    auto reader = cgl::IAnimeInfoReader::create({&settings, version, bytes.size(), 1026});
    ASSERT_EQ(reader->load(), cgl::Results::Success);
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/AnimeInfo_4.bin"));
    for (uint32_t i = 0; i < 1025; ++i) {
        cgl::AnimeResourceInfo out{};
        ASSERT_EQ(reader->query({version, 100 + i}, &out), cgl::Results::Success);
        EXPECT_EQ(out.dataOffset, i ? i * 10 : 9999);
        EXPECT_EQ(out.motionCount, i ? 2 : 3);
    }
    std::vector<cgl::AnimeResourceSerialNum> serials;
    ASSERT_EQ(reader->queryAvailableSerialNums(&serials), cgl::Results::Success);
    EXPECT_EQ(serials.size(), 1025);
    reader->close(); reader->close();
    serials.clear();
    reader->queryAvailableSerialNums(&serials);
    EXPECT_TRUE(serials.empty());
}
TEST_F(ResourceReaderTest, DefaultIndexLimitsRejectLargeInputsBeforePreloading) {
    write("GraphicInfo_66.bin", {0});
    std::filesystem::resize_file(dir / "bin/GraphicInfo_66.bin", (1024ull * 1024 + 1) * 40);
    auto graphics = cgl::IGraphicsInfoReader::create({&settings, version});
    EXPECT_EQ(graphics->load(), cgl::Results::InvalidFile);
    EXPECT_EQ(graphics->infoCount(), 0);
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/GraphicInfo_66.bin"));
    write("AnimeInfo_4.bin", {0});
    std::filesystem::resize_file(dir / "bin/AnimeInfo_4.bin", 65537ull * 12);
    auto anime = cgl::IAnimeInfoReader::create({&settings, version});
    EXPECT_EQ(anime->load(), cgl::Results::InvalidFile);
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/AnimeInfo_4.bin"));
}
