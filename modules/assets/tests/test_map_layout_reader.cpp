#include "cgl/assets/map_layout_reader.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>

namespace {
class MapLayoutReaderTest : public testing::Test {
protected:
    std::filesystem::path directory;
    cgl::IMapLayoutReader::Ptr reader = cgl::IMapLayoutReader::create();
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("cgl-map-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
    }
    void TearDown() override { std::filesystem::remove_all(directory); }
    std::vector<unsigned char> map(uint32_t width = 2, uint32_t height = 3) {
        std::vector<unsigned char> bytes(20, 0);
        bytes[0] = 'M'; bytes[1] = 'A'; bytes[2] = 'P';
        for (int i = 0; i < 4; ++i) {
            bytes[12 + i] = static_cast<unsigned char>(width >> (8 * i));
            bytes[16 + i] = static_cast<unsigned char>(height >> (8 * i));
        }
        if (width == 2 && height == 3) {
            for (uint16_t value = 0; value < 18; ++value) {
                const uint16_t encoded = static_cast<uint16_t>(0xc000 + value);
                bytes.push_back(static_cast<unsigned char>(encoded));
                bytes.push_back(static_cast<unsigned char>(encoded >> 8));
            }
        }
        return bytes;
    }
    cgl::Results read(const std::vector<unsigned char>& bytes, cgl::MapLayout* output) {
        const auto path = directory / "fixture.dat";
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        return reader->read(path, output);
    }
};

TEST_F(MapLayoutReaderTest, RectangularMapPreservesPlanesAndRawValues) {
    cgl::MapLayout layout;
    ASSERT_EQ(read(map(), &layout), cgl::Results::Success);
    EXPECT_EQ(layout.width, 2);
    EXPECT_EQ(layout.height, 3);
    ASSERT_EQ(layout.cells.size(), 6);
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 2; ++x) {
        const auto* cell = layout.cell(x, y);
        ASSERT_NE(cell, nullptr);
        EXPECT_EQ(cell->groundId, 0xc000 + y * 2 + x);
        EXPECT_EQ(cell->objectId, 0xc006 + y * 2 + x);
        EXPECT_EQ(cell->flags, 0xc00c + y * 2 + x);
    }
    EXPECT_EQ(layout.cell(-1, 0), nullptr);
    EXPECT_EQ(layout.cell(0, -1), nullptr);
    EXPECT_EQ(layout.cell(2, 0), nullptr);
    EXPECT_EQ(layout.cell(0, 3), nullptr);
}

TEST_F(MapLayoutReaderTest, RejectsEveryTruncationWithoutChangingDestination) {
    const auto bytes = map();
    cgl::MapLayout layout{1, 1, {{123, 456, 789}}};
    for (size_t length = 0; length < bytes.size(); ++length) {
        EXPECT_EQ(read(std::vector<unsigned char>(bytes.begin(), bytes.begin() + length), &layout),
            cgl::Results::InvalidFile) << length;
        ASSERT_EQ(layout.cells.size(), 1);
        EXPECT_EQ(layout.width, 1);
        EXPECT_EQ(layout.height, 1);
        EXPECT_EQ(layout.cell(0, 0)->flags, 789);
    }
}

TEST_F(MapLayoutReaderTest, RejectsBadMagicTrailingBytesAndUnsafeDimensions) {
    cgl::MapLayout layout;
    auto bytes = map(); bytes[0] = 'X';
    EXPECT_EQ(read(bytes, &layout), cgl::Results::InvalidFile);
    bytes = map(); bytes.push_back(0);
    EXPECT_EQ(read(bytes, &layout), cgl::Results::InvalidFile);
    for (const auto dimensions : {std::pair{0u, 3u}, {2u, 0u}, {65536u, 65536u},
            {0xffffffffu, 0xffffffffu}, {4096u, 4096u}}) {
        EXPECT_EQ(read(map(dimensions.first, dimensions.second), &layout), cgl::Results::InvalidFile);
    }
}

TEST_F(MapLayoutReaderTest, HandlesMissingFileAndNullDestination) {
    cgl::MapLayout layout;
    EXPECT_EQ(reader->read(directory / "missing.dat", &layout), cgl::Results::InvalidFile);
    EXPECT_EQ(read(map(), nullptr), cgl::Results::InvalidArgs);
    EXPECT_EQ(layout.cell(0, 0), nullptr);
}
}
