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
#include "cgl/assets/graphics_resource_file_info_reader.h"
#include "cgl/assets/graphics_resource_file_data_reader.h"

// -----------------------------------------------------------------------------
namespace {

class IGraphicsResourceFileDataReaderTest : public testing::Test {
 protected:
    void SetUp() override {
        // the path of test settings.ini is configured via CMakeLists.txt
        // > gtest_discover_tests(${EXE_NAME} WORKING_DIRECTORY ${CGL_BIN_PATH})
        pSettings_ = cgl::LoadRuntimeSettings("settings.ini");
        EXPECT_NE(pSettings_, nullptr);

        // create graphx resource info reader
        infoReader_ = cgl::IGraphicsResourceFileInfoReader::create({
            .pSettings = pSettings_.get(),
            .version   = cgl::CrossGateVersion::CG_VERSION_Classic
        });
        EXPECT_EQ(infoReader_->load(), cgl::Results::Success);

         cgl::GraphicsResourceSerialNum idx {
            .type    = cgl::GraphicsAssetsSerialNumTypes::GraphicsSerialNum,
            .version = cgl::CrossGateVersion::CG_VERSION_Classic,
            .value   = 0
        };
        EXPECT_EQ(infoReader_->query(idx, &info_), cgl::Results::Success);
    }

    cgl::RuntimeSettingsPtr pSettings_;
    cgl::IGraphicsResourceFileInfoReader::Ptr infoReader_;
    cgl::GraphicsResourceInfo info_;
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(IGraphicsResourceFileDataReaderTest, ReaderInitAndLoad) {
    cgl::IGraphicsResourceFileDataReader::CreateInfo createInfo {
        .pSettings = pSettings_.get(),
        .version   = cgl::CrossGateVersion::CG_VERSION_Classic
    };
    auto reader = cgl::IGraphicsResourceFileDataReader::create(createInfo);
    EXPECT_NE(reader, nullptr);

    // Try to load the version data.
    EXPECT_EQ(reader->load(), cgl::Results::Success);

    // Try to query graphic data
    cgl::GraphicsResourceData gfxResData;
    EXPECT_EQ(reader->query(info_, &gfxResData), cgl::Results::Success);
    EXPECT_EQ(gfxResData.version, info_.version);
    EXPECT_EQ(gfxResData.width, info_.width);
    EXPECT_EQ(gfxResData.height, info_.height);
    EXPECT_NE(gfxResData.pData, nullptr);
    EXPECT_EQ(gfxResData.pPaletteData, nullptr);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
