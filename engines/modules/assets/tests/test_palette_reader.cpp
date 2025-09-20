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
#include "cgl/assets/palette_reader.h"

// -----------------------------------------------------------------------------
namespace {

class IPaletteReaderTest : public testing::Test {
 protected:
    void SetUp() override {
        pSettings_ = cgl::LoadRuntimeSettings("settings.ini");
        EXPECT_NE(pSettings_, nullptr);
    }

    cgl::RuntimeSettingsPtr pSettings_;
};

}   // namespace

// -----------------------------------------------------------------------------
TEST_F(IPaletteReaderTest, ReaderCreation) {
    // invalid creation
    EXPECT_EQ(cgl::IPaletteReader::create({.pSettings = nullptr}), nullptr);

    // valid creation
    EXPECT_NE(cgl::IPaletteReader::create({.pSettings = pSettings_.get()}), nullptr);
}


TEST_F(IPaletteReaderTest, ReadData) {
    auto reader = cgl::IPaletteReader::create({.pSettings = pSettings_.get()});
    cgl::PaletteData256 paletteData;

    EXPECT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &paletteData),
              cgl::Results::Success);
    EXPECT_EQ(paletteData.size() , 256);    // Test for 8bit color (256)
}

TEST_F(IPaletteReaderTest, CheckGlobalPaletteDefaultPaletteData) {
    // Load data to check if the loading work find
    auto reader = cgl::IPaletteReader::create({.pSettings = pSettings_.get()});
    cgl::PaletteData256 paleteA;
    cgl::PaletteData256 paleteB;

    EXPECT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Daytime, &paleteA),
              cgl::Results::Success);
    EXPECT_EQ(reader->read(cgl::EnvironmentPaletteTypes::Evening, &paleteB),
              cgl::Results::Success);

    // The following two parts of the array are fixed values in the global
    // palettes.
    for (int32_t i = 0 ; i <= 15; i++) {
        EXPECT_EQ(paleteA[i].BGR.R, paleteB[i].BGR.R);
        EXPECT_EQ(paleteA[i].BGR.G, paleteB[i].BGR.G);
        EXPECT_EQ(paleteA[i].BGR.B, paleteB[i].BGR.B);
    }
    for (int32_t i = 240 ; i <= 255; i++) {
        EXPECT_EQ(paleteA[i].BGR.R, paleteB[i].BGR.R);
        EXPECT_EQ(paleteA[i].BGR.G, paleteB[i].BGR.G);
        EXPECT_EQ(paleteA[i].BGR.B, paleteB[i].BGR.B);
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
