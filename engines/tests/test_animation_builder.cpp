#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "assets/animation_builder.h"
#include "assets/assets_components.h"
#include "cgl/settings/settings.h"

class AnimationBuilderTest : public testing::Test {
protected:
    static constexpr auto version = cgl::CrossGateVersion::CG_VERSION_Classic;
    std::filesystem::path dir;
    cgl::Settings settings{};
    cgl::component::AssetsReaderState readers{};
    cgl::AnimationBuilder builder;
    std::vector<uint8_t> graphics, index;
    const cgl::AnimationRequest request{100000, 0, 0};
    static void word(std::vector<uint8_t>& bytes, uint32_t value, int width = 4) {
        for (int i = 0; i < width; ++i) bytes.push_back(uint8_t(value >> (i * 8)));
    }
    void write(const char* name, const std::vector<uint8_t>& bytes) {
        std::ofstream f(dir / "bin" / name, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
            ("cgl-animation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir / "bin/pal");
        settings.general.EngineRootPath = dir.string();
        for (uint32_t i = 0; i < 2; ++i) {
            word(index, 100 + i); word(index, uint32_t(graphics.size())); word(index, 20);
            word(index, i ? 1 : uint32_t(-1)); word(index, i ? 0 : uint32_t(-2));
            word(index, 2); word(index, 2); index.insert(index.end(), 12, 0);
            graphics.insert(graphics.end(), {'R','D',0,0});
            word(graphics, 2); word(graphics, 2); word(graphics, 20);
            graphics.insert(graphics.end(), {16,17,18,0});
        }
        write("GraphicInfo_66.bin", index); write("Graphic_66.bin", graphics);
        std::vector<uint8_t> anime, animeIndex;
        for (uint32_t i = 0; i < 2; ++i) {
            word(anime, i, 2); word(anime, 0, 2); word(anime, 100 + i * 100); word(anime, 2);
            for (uint32_t serial : {100, 101}) { word(anime, serial); anime.insert(anime.end(), 6, 0); }
            word(animeIndex, 100000 + i); word(animeIndex, 0); word(animeIndex, 2, 2); word(animeIndex, 0, 2);
        }
        write("Anime_4.bin", anime); write("AnimeInfo_4.bin", animeIndex);
        std::vector<uint8_t> palette(708, 0);
        for (int i = 0; i < 9; ++i) palette[i] = uint8_t((i + 1) * 10);
        write("pal/palet_00.cgp", palette);
        readers.pAnimeInfoReader[version] = cgl::IAnimeInfoReader::create({&settings, version});
        readers.pAnimeDataReader[version] = cgl::IAnimeDataReader::create({&settings, version, 0});
        readers.pGfxInfoReader[version] = cgl::IGraphicsInfoReader::create({&settings, version});
        readers.pGfxDataReader[version] = cgl::IGraphicsDataReader::create({&settings, version, 0});
        readers.pPaletteReader = cgl::IPaletteReader::create({&settings});
        ASSERT_EQ(readers.pAnimeInfoReader[version]->load(), cgl::Results::Success);
        ASSERT_EQ(readers.pAnimeDataReader[version]->load(), cgl::Results::Success);
        ASSERT_EQ(readers.pGfxInfoReader[version]->load(), cgl::Results::Success);
        ASSERT_EQ(readers.pGfxDataReader[version]->load(), cgl::Results::Success);
        readers.pGfxDataReader[version]->resetCacheStatistics();
    }
    void TearDown() override {
        builder.clear();
        for(unsigned i=0;i<unsigned(cgl::CrossGateVersion::COUNT);++i) {
            const auto v=cgl::CrossGateVersion(i);
            if(readers.pAnimeInfoReader[v]) readers.pAnimeInfoReader[v]->close();
            if(readers.pAnimeDataReader[v]) readers.pAnimeDataReader[v]->close();
            if(readers.pGfxInfoReader[v]) readers.pGfxInfoReader[v]->close();
            if(readers.pGfxDataReader[v]) readers.pGfxDataReader[v]->close();
        }
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        EXPECT_FALSE(ec) << ec.message();
    }
    void reloadIndex() {
        readers.pGfxInfoReader[version]->close(); write("GraphicInfo_66.bin", index);
        ASSERT_EQ(readers.pGfxInfoReader[version]->load(), cgl::Results::Success);
    }
};
TEST_F(AnimationBuilderTest, AlignsBottomUpFramesAndPreservesTransparencyAndColors) {
    auto result = builder.build(request, readers, 128);
    EXPECT_EQ(result.width, 4); EXPECT_EQ(result.height, 4); EXPECT_EQ(result.frameCount, 2);
    EXPECT_EQ(result.durationMs, 100); EXPECT_EQ(result.offsetX, -1); EXPECT_EQ(result.offsetY, -2);
    ASSERT_EQ(result.rgba.size(), 128);
    const auto& p = result.rgba;
    EXPECT_EQ(std::vector<uint8_t>(p.begin(), p.begin() + 4), (std::vector<uint8_t>{90,80,70,255}));
    EXPECT_EQ(p[7], 0); // Transparent source pixel.
    EXPECT_EQ(p[11], 0); // Canvas padding.
    EXPECT_EQ(std::vector<uint8_t>(p.begin() + 16, p.begin() + 20), (std::vector<uint8_t>{30,20,10,255}));
    const size_t secondTopLeft = 64 + (2 * 4 + 2) * 4;
    EXPECT_EQ(p[secondTopLeft], 90); EXPECT_EQ(p[secondTopLeft + 3], 255);
}
TEST_F(AnimationBuilderTest, ReusesOneDescriptionAcrossDirectionsAndInvalidatesOnSerialChange) {
    builder.build(request, readers);
    auto* data = readers.pAnimeDataReader[version].get();
    data->resetCacheStatistics();
    auto second = builder.build({100000, 1, 0}, readers);
    EXPECT_EQ(second.durationMs, 200);
    EXPECT_EQ(data->cacheStatistics().fileReadCalls, 0);
    builder.build({100001, 0, 0}, readers);
    EXPECT_GT(data->cacheStatistics().fileReadCalls, 0);
    data->resetCacheStatistics();
    builder.build(request, readers); // Switching back reparses rather than accumulating descriptions.
    EXPECT_GT(data->cacheStatistics().fileReadCalls, 0);
    data->resetCacheStatistics(); builder.clear(); builder.clear();
    builder.build(request, readers);
    EXPECT_GT(data->cacheStatistics().fileReadCalls, 0);
}
TEST_F(AnimationBuilderTest, ReaderReplacementInvalidatesDescription) {
    builder.build(request, readers);
    auto replacement = cgl::IAnimeDataReader::create({&settings, version, 0});
    ASSERT_EQ(replacement->load(), cgl::Results::Success);
    readers.pAnimeDataReader[version] = std::move(replacement);
    builder.build({100000, 1, 0}, readers);
    EXPECT_GT(readers.pAnimeDataReader[version]->cacheStatistics().fileReadCalls, 0);
}
TEST_F(AnimationBuilderTest, RejectsBudgetBeforeDecodingAnyFrameOrReadingPalette) {
    ASSERT_TRUE(std::filesystem::remove(dir / "bin/pal/palet_00.cgp"));
    EXPECT_THROW(builder.build(request, readers, 127), std::runtime_error);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls, 0);
    EXPECT_THROW(builder.build(request, readers, 0), std::runtime_error);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls, 0);
}
TEST_F(AnimationBuilderTest, RejectsInvalidLastFrameAndExtremeOffsetsBeforeDecoding) {
    // The first frame is valid; failure in later metadata must still precede every decode.
    for (int i = 60; i < 64; ++i) index[i] = 0; // Second frame width.
    reloadIndex();
    EXPECT_THROW(builder.build(request, readers), std::runtime_error);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls, 0);
    index[60] = 2;
    index[52] = 255; index[53] = 255; index[54] = 255; index[55] = 127;
    reloadIndex();
    EXPECT_THROW(builder.build(request, readers), std::runtime_error);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls, 0);
}
TEST_F(AnimationBuilderTest, MidDecodeFailureCanRetryWithoutReplacingPreviouslyReturnedPixels) {
    const auto previous = builder.build(request, readers);
    readers.pGfxDataReader[version]->close();
    auto broken = graphics; broken[24] = 1; // Second RD header width mismatches metadata.
    write("Graphic_66.bin", broken);
    ASSERT_EQ(readers.pGfxDataReader[version]->load(), cgl::Results::Success);
    EXPECT_THROW(builder.build(request, readers), std::runtime_error);
    EXPECT_EQ(previous.rgba[0], 90);
    readers.pGfxDataReader[version]->close(); write("Graphic_66.bin", graphics);
    ASSERT_EQ(readers.pGfxDataReader[version]->load(), cgl::Results::Success);
    EXPECT_EQ(builder.build(request, readers).rgba, previous.rgba);
    for (int i = 0; i < 100; ++i) {
        builder.clear();
        EXPECT_EQ(builder.build(request, readers).rgba, previous.rgba);
    }
}
TEST_F(AnimationBuilderTest, MissingReadersAndUnavailableMotionsFailCleanly) {
    cgl::component::AssetsReaderState empty{};
    EXPECT_THROW(builder.build(request, empty), std::runtime_error);
    EXPECT_THROW(builder.build({100000, 999, 0}, readers), std::runtime_error);
    EXPECT_THROW(builder.build({100000, 0, 999}, readers), std::runtime_error);
    EXPECT_THROW(builder.build({100000, 2, 0}, readers), std::runtime_error);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls, 0);
}

TEST_F(AnimationBuilderTest, VersionIsPartOfDescriptionAndRequestIdentity) {
    builder.build(request,readers);
    const auto other=cgl::CrossGateVersion::CG_VERSION_PUK1;
    std::filesystem::copy_file(dir/"bin/AnimeInfo_4.bin",dir/"bin/AnimeInfoEx_1.Bin");
    std::filesystem::copy_file(dir/"bin/Anime_4.bin",dir/"bin/AnimeEx_1.Bin");
    std::filesystem::copy_file(dir/"bin/GraphicInfo_66.bin",dir/"bin/GraphicInfoEx_5.bin");
    std::filesystem::copy_file(dir/"bin/Graphic_66.bin",dir/"bin/GraphicEx_5.bin");
    { std::fstream f(dir/"bin/AnimeEx_1.Bin",std::ios::in|std::ios::out|std::ios::binary);
      f.seekp(4);const char duration[4]={9,3,0,0};f.write(duration,4); }
    readers.pAnimeInfoReader[other]=cgl::IAnimeInfoReader::create({&settings,other});
    readers.pAnimeDataReader[other]=cgl::IAnimeDataReader::create({&settings,other,0});
    readers.pGfxInfoReader[other]=cgl::IGraphicsInfoReader::create({&settings,other});
    readers.pGfxDataReader[other]=cgl::IGraphicsDataReader::create({&settings,other,0});
    ASSERT_EQ(readers.pAnimeInfoReader[other]->load(),cgl::Results::Success);
    ASSERT_EQ(readers.pAnimeDataReader[other]->load(),cgl::Results::Success);
    ASSERT_EQ(readers.pGfxInfoReader[other]->load(),cgl::Results::Success);
    ASSERT_EQ(readers.pGfxDataReader[other]->load(),cgl::Results::Success);
    auto alternate=request;alternate.version=other;
    EXPECT_NE(alternate,request);
    EXPECT_EQ(builder.build(alternate,readers).durationMs,777);
    EXPECT_EQ(builder.build(request,readers).durationMs,100);
    alternate.version=cgl::CrossGateVersion::COUNT;
    EXPECT_THROW(builder.build(alternate,readers),std::runtime_error);
}

TEST_F(AnimationBuilderTest, PreCancelledWorkNeverTouchesUnavailableReaders) {
    cgl::component::AssetsReaderState readers{};
    cgl::AnimationBuilder builder;
    std::atomic_bool cancelled{true};
    try {builder.build({100000,0,0},readers,32*1024*1024,&cancelled);FAIL();}
    catch(const std::runtime_error& error) {EXPECT_STREQ(error.what(),"Animation decode cancelled");}
}

TEST_F(AnimationBuilderTest, ReportsRequiredPayloadWithoutReadingPixelsForTransientBudget) {
    try {builder.build(request,readers,127);FAIL()<<"Expected budget admission failure";}
    catch(const cgl::AnimationBudgetExceeded& error) {EXPECT_EQ(error.requiredBytes,128);}
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls,0);
    EXPECT_EQ(builder.build(request,readers,128).rgba.size(),128);
}

TEST_F(AnimationBuilderTest, RequiredBytesPlansWithoutPaletteOrPixelReads) {
    ASSERT_TRUE(std::filesystem::remove(dir/"bin/pal/palet_00.cgp"));
    EXPECT_EQ(builder.requiredBytes(request,readers),128);
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls,0);
    EXPECT_THROW(builder.build(request,readers),std::runtime_error);
}
