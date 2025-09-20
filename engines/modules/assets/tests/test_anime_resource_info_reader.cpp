// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "cgl/settings/settings.h"
#include "cgl/assets/anime_resource_info_reader.h"

// -----------------------------------------------------------------------------
namespace {

class IAnimeResourceInfoReaderTest : public testing::Test {
 protected:
    void SetUp() override {
        pSettings_ = cgl::LoadRuntimeSettings("settings.ini");
        EXPECT_NE(pSettings_, nullptr);
    }

    cgl::RuntimeSettingsPtr pSettings_;
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(IAnimeResourceInfoReaderTest, ReaderInit) {
    // invalid init args check
    EXPECT_EQ(cgl::IAnimeResourceInfoReader::create({
                .pSettings = nullptr,
                .version   = cgl::CrossGateVersion::UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IAnimeResourceInfoReader::create({
                .pSettings = pSettings_.get(),
                .version   = cgl::CrossGateVersion::UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IAnimeResourceInfoReader::create({
                .pSettings = pSettings_.get(),
                .version   = cgl::CrossGateVersion::COUNT
              }), nullptr);

    // valid creation
    cgl::IAnimeResourceInfoReader::CreateInfo cInfo {
        .pSettings = pSettings_.get(),
        .version   = cgl::CrossGateVersion::CG_VERSION_PUK1
    };
    auto reader = cgl::IAnimeResourceInfoReader::create(cInfo);
    EXPECT_NE(reader, nullptr);
    EXPECT_EQ(reader->createInfo().pSettings, cInfo.pSettings);
    EXPECT_EQ(reader->createInfo().version, cInfo.version);
}

// -----------------------------------------------------------------------------
TEST_F(IAnimeResourceInfoReaderTest, LoadData) {
    auto reader = cgl::IAnimeResourceInfoReader::create({
      .pSettings = pSettings_.get(),
      .version   = cgl::CrossGateVersion::CG_VERSION_Classic
    });
    EXPECT_NE(reader, nullptr);

    // Try to load the version data.
    cgl::Results result = reader->load();
    EXPECT_EQ(result, cgl::Results::Success);

    // Check if the reader has loaded data.
    std::vector<cgl::AnimeResourceSerialNum> serialNums;
    result = reader->queryAvailableSerialNums(&serialNums);
    EXPECT_EQ(result, cgl::Results::Success);

    // Try to query data
    cgl::AnimeResourceInfo info;
    for (const auto& idx : serialNums) {
        EXPECT_EQ(idx.version, cgl::CrossGateVersion::CG_VERSION_Classic);
        EXPECT_GT(idx.value, 0);

        // Query the info
        result = reader->query(idx, &info);
        EXPECT_EQ(result, cgl::Results::Success);
        EXPECT_EQ(info.serialNum.version, idx.version);
        EXPECT_EQ(info.serialNum.value, idx.value);
        EXPECT_GE(info.dataOffset, 0);
        EXPECT_GT(info.motionCount, 0);
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
