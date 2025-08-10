// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "cgl/settings/settings_loader.h"
#include "cgl/settings/settings.h"
#include "cgl/resources/anime_resource_info_reader.h"
#include "cgl/resources/anime_resource_data_reader.h"

// -----------------------------------------------------------------------------
namespace {

class IAnimeResourceDataReaderTest : public testing::Test {
 protected:
    void SetUp() override {
        cgl::SettingsLoader loader;
        EXPECT_EQ(loader.settings(), nullptr);

        settingLoader.load();
        pSettings_ = settingLoader.settings();

        // prepare info reader
        infoReader_ = cgl::IAnimeResourceInfoReader::create({
            .pSettings = pSettings_,
            .version   = cgl::CrossGateVersion::CG_VERSION_Classic
        });
        EXPECT_NE(infoReader_, nullptr);
        EXPECT_EQ(infoReader_->load(), cgl::Results::Success);
    }

    const cgl::Settings* pSettings_;
    cgl::SettingsLoader settingLoader;
    cgl::IAnimeResourceInfoReader::Ptr infoReader_;
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(IAnimeResourceDataReaderTest, ReaderInit) {
    // invalid init args check
    EXPECT_EQ(cgl::IAnimeResourceDataReader::create({
                .pSettings = nullptr,
                .version   = cgl::CrossGateVersion::CG_VERSION_UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IAnimeResourceDataReader::create({
                .pSettings = pSettings_,
                .version   = cgl::CrossGateVersion::CG_VERSION_UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IAnimeResourceDataReader::create({
                .pSettings = pSettings_,
                .version   = cgl::CrossGateVersion::Count
              }), nullptr);

    // valid creation
    cgl::IAnimeResourceDataReader::CreateInfo cInfo {
        .pSettings = pSettings_,
        .version   = cgl::CrossGateVersion::CG_VERSION_PUK1
    };
    auto reader = cgl::IAnimeResourceDataReader::create(cInfo);
    EXPECT_NE(reader, nullptr);
    EXPECT_EQ(reader->createInfo().pSettings, cInfo.pSettings);
    EXPECT_EQ(reader->createInfo().version, cInfo.version);
}

// -----------------------------------------------------------------------------
TEST_F(IAnimeResourceDataReaderTest, LoadData) {
    // prepare info
    cgl::AnimeResourceInfo animeResInfo;
    infoReader_->query(
        cgl::AnimeResourceSerialNum{
            .version = cgl::CrossGateVersion::CG_VERSION_Classic,
            .value   = 100000
        },
        &animeResInfo);

    // Create the data reader
    auto reader = cgl::IAnimeResourceDataReader::create({
      .pSettings = pSettings_,
      .version   = cgl::CrossGateVersion::CG_VERSION_Classic
    });
    EXPECT_NE(reader, nullptr);

    // Try to load the version data.
    cgl::Results result = reader->load();
    EXPECT_EQ(result, cgl::Results::Success);

    // Try to query data
    cgl::AnimeResourceData animeData;
    result = reader->query(animeResInfo, &animeData);
    EXPECT_EQ(result, cgl::Results::Success);

    // Check the data
    EXPECT_EQ(animeData.serialNum.version, animeResInfo.serialNum.version);
    EXPECT_EQ(animeData.serialNum.value, animeResInfo.serialNum.value);
    EXPECT_EQ(animeData.motionMap.size(), animeResInfo.motionCount);
    for (const auto& [key, motionDesc] : animeData.motionMap) {
        EXPECT_EQ(motionDesc.version, animeResInfo.serialNum.version);
        EXPECT_EQ(motionDesc.direction, key.first);
        EXPECT_EQ(motionDesc.motion, key.second);
        EXPECT_GT(motionDesc.motionGraphicsSerialNums.size(), 0);
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
