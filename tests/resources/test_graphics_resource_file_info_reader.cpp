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
#include "cgl/resources/graphics_resource_file_info_reader.h"

// -----------------------------------------------------------------------------
namespace {

class IGraphicsResourceFileInfoReaderTest : public testing::Test {
 protected:
    void SetUp() override {
        cgl::SettingsLoader loader;
        EXPECT_EQ(loader.settings(), nullptr);

        settingLoader.load();
        pSettings_ = settingLoader.settings();
    }

    const cgl::Settings* pSettings_;
    cgl::SettingsLoader settingLoader;
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(IGraphicsResourceFileInfoReaderTest, ReaderInit) {
    // invalid init args check
    EXPECT_EQ(cgl::IGraphicsResourceFileInfoReader::create({
                .pSettings = nullptr,
                .version   = cgl::CrossGateVersion::CG_VERSION_UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IGraphicsResourceFileInfoReader::create({
                .pSettings = pSettings_,
                .version   = cgl::CrossGateVersion::CG_VERSION_UNKNOWN
              }), nullptr);

    EXPECT_EQ(cgl::IGraphicsResourceFileInfoReader::create({
                .pSettings = pSettings_,
                .version   = cgl::CrossGateVersion::Count
              }), nullptr);

    // valid creation
    cgl::IGraphicsResourceFileInfoReader::CreateInfo cInfo {
        .pSettings = pSettings_,
        .version   = cgl::CrossGateVersion::CG_VERSION_PUK1
    };
    auto reader = cgl::IGraphicsResourceFileInfoReader::create(cInfo);
    EXPECT_NE(reader, nullptr);
    EXPECT_EQ(reader->infoCount(), 0);
    EXPECT_EQ(reader->createInfo().pSettings, cInfo.pSettings);
    EXPECT_EQ(reader->createInfo().version, cInfo.version);
}

// -----------------------------------------------------------------------------
TEST_F(IGraphicsResourceFileInfoReaderTest, LoadData) {
    auto reader = cgl::IGraphicsResourceFileInfoReader::create({
      .pSettings = pSettings_,
      .version   = cgl::CrossGateVersion::CG_VERSION_Classic
    });
    EXPECT_NE(reader, nullptr);

    // Try to load the version data.
    cgl::Results result = reader->load();
    EXPECT_EQ(result, cgl::Results::Success);
    EXPECT_GE(reader->infoCount(), 0);

    // Try to query data
    cgl::GraphicsResourceSerialNum idx {
        .type    = cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum,
        .version = cgl::CrossGateVersion::CG_VERSION_Classic,
        .value   = 0
    };
    cgl::GraphicsResourceInfo info;
    result = reader->query(idx, &info);
    EXPECT_EQ(result, cgl::Results::Success);
    EXPECT_EQ(info.gfxBasedsIdx, idx);
    EXPECT_EQ(info.width, 64);      // resource width in classic always be 64
    EXPECT_EQ(info.height, 47);     // resource height in classic always be 47
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
