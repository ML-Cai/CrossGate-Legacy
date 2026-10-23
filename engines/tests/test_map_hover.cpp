#include "map/map_hover.h"
#include <gtest/gtest.h>
#include <limits>

namespace {
cgl::MapLayout Ground(uint32_t width = 3, uint32_t height = 2) {
    return {width, height, std::vector<cgl::MapCell>(size_t(width) * height, {123,0,0})};
}

TEST(MapHoverTest, PicksCellCentersAcrossCameraLocationsAndScales) {
    const auto map = Ground(64, 32);
    const cgl::Viewport viewport{17,23,800,600};
    for (double scale : {0.125, 1.0, 2.0, 16.0}) {
        for (auto center : {cgl::MapPoint{0,0}, {17.25,9.5}, {63,31}}) {
            const cgl::component::MapCamera camera{center, scale};
            unsigned checked = 0;
            for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) {
                const auto screen = cgl::MapToScreen({double(x),double(y)}, camera, viewport);
                if (screen.x < 0 || screen.y < 0 || screen.x >= 800 || screen.y >= 600) continue;
                const auto selected = cgl::PickMapCell(screen, camera, viewport, map);
                ASSERT_TRUE(selected);
                EXPECT_EQ(*selected, (cgl::MapCellCoord{x,y}));
                ++checked;
            }
            EXPECT_GT(checked, 0);
        }
    }
}

TEST(MapHoverTest, DiamondEdgesUseHalfOpenCellsIncludingNegativeMapCoordinates) {
    const auto map = Ground();
    const cgl::Viewport viewport{0,0,1000,800};
    const cgl::component::MapCamera camera{{0,0},1};
    auto pick = [&](double x, double y) {
        return cgl::PickMapCell(cgl::MapToScreen({x,y}, camera, viewport), camera, viewport, map);
    };
    EXPECT_EQ(pick(0.49999,0.49999), (cgl::MapCellCoord{0,0}));
    EXPECT_EQ(pick(0.5,0), (cgl::MapCellCoord{1,0}));
    EXPECT_EQ(pick(-0.49999,0), (cgl::MapCellCoord{0,0}));
    EXPECT_FALSE(pick(-0.50001,0));
    EXPECT_EQ(pick(2.49999,1), (cgl::MapCellCoord{2,1}));
    EXPECT_FALSE(pick(2.5,1));
    EXPECT_EQ(pick(0,0.5), (cgl::MapCellCoord{0,1}));
    EXPECT_FALSE(pick(0,1.5));
    // Four exact screen-space corners of tile (0,0).
    EXPECT_EQ(cgl::PickMapCell({532,400},camera,viewport,map), (cgl::MapCellCoord{1,1}));
    EXPECT_EQ(cgl::PickMapCell({500,376.5},camera,viewport,map), (cgl::MapCellCoord{1,0}));
    EXPECT_EQ(cgl::PickMapCell({500,423.5},camera,viewport,map), (cgl::MapCellCoord{0,1}));
    EXPECT_EQ(cgl::PickMapCell({468,400},camera,viewport,map), (cgl::MapCellCoord{0,0}));
}

TEST(MapHoverTest, RejectsEmptyGroundInvalidCoordinatesAndViewportEdges) {
    auto map = Ground();
    map.cells[0].groundId = 0;
    map.cells[1].groundId = 2;
    map.cells[2].flags = 193; // Hover is visual selection, not a walkability check.
    const cgl::Viewport viewport{0,0,800,600};
    cgl::component::MapCamera camera{{0,0},1};
    EXPECT_FALSE(cgl::PickMapCell(cgl::MapToScreen({0,0},camera,viewport),camera,viewport,map));
    EXPECT_FALSE(cgl::PickMapCell(cgl::MapToScreen({1,0},camera,viewport),camera,viewport,map));
    EXPECT_EQ(cgl::PickMapCell(cgl::MapToScreen({2,0},camera,viewport),camera,viewport,map), (cgl::MapCellCoord{2,0}));
    for (auto point : {cgl::MapPoint{-1,300}, {800,300}, {400,600}, {400,-1},
            {std::numeric_limits<double>::quiet_NaN(),300}})
        EXPECT_FALSE(cgl::PickMapCell(point,camera,viewport,map));
    camera.scale = 0;
    EXPECT_FALSE(cgl::PickMapCell({400,300},camera,viewport,map));
    camera.scale = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(cgl::PickMapCell({400,300},camera,viewport,map));
    camera.scale = 1;
    EXPECT_FALSE(cgl::PickMapCell({400,300},camera,viewport,{}));
}

TEST(MapHoverTest, StationaryCursorTracksCameraPanAndScale) {
    const auto map = Ground(20,20);
    const cgl::Viewport viewport{0,0,800,600};
    cgl::component::MapCamera camera{{5,5},1};
    EXPECT_EQ(cgl::PickMapCell({400,300},camera,viewport,map), (cgl::MapCellCoord{5,5}));
    camera.center = {6,5};
    EXPECT_EQ(cgl::PickMapCell({400,300},camera,viewport,map), (cgl::MapCellCoord{6,5}));
    EXPECT_EQ(cgl::PickMapCell({528,300},camera,viewport,map), (cgl::MapCellCoord{8,7}));
    camera.scale = 2;
    EXPECT_EQ(cgl::PickMapCell({528,300},camera,viewport,map), (cgl::MapCellCoord{7,6}));
}

TEST(MapHoverTest, ConvertsLogicalCursorToRenderPixelsAndViewportOrigin) {
    const cgl::Viewport viewport{100,200,800,600};
    const auto point = cgl::WindowToViewport({250,250},{800,600},{1600,1200},viewport);
    ASSERT_TRUE(point);
    EXPECT_DOUBLE_EQ(point->x, 400);
    EXPECT_DOUBLE_EQ(point->y, 300);
    const auto map = Ground(20,20);
    EXPECT_EQ(cgl::PickMapCell(*point,{{5,5},2},viewport,map), (cgl::MapCellCoord{5,5}));
    EXPECT_FALSE(cgl::WindowToViewport({0,0},{800,600},{1600,1200},viewport));
    EXPECT_FALSE(cgl::WindowToViewport({800,300},{800,600},{1600,1200},viewport));
    EXPECT_FALSE(cgl::WindowToViewport({400,300},{0,600},{1600,1200},viewport));
    EXPECT_FALSE(cgl::WindowToViewport({400,300},{800,600},{0,1200},viewport));
    EXPECT_FALSE(cgl::WindowToViewport({400,300},{800,600},{800,600},viewport));
}

TEST(MapHoverTest, OutlineStaysHollowAndVisibleAtSupportedScales) {
    for (double scale : {0.125, 0.5, 1.0, 2.0, 16.0}) {
        const auto image = cgl::BuildMapHoverBitmap(scale);
        ASSERT_EQ(image.rgba.size(), size_t(image.width) * image.height * 4);
        auto alpha = [&](uint32_t x, uint32_t y) { return image.rgba[(size_t(y)*image.width+x)*4+3]; };
        EXPECT_EQ(alpha(image.width/2,image.height/2),0);
        for (uint32_t x = 0; x < image.width; ++x) {
            EXPECT_EQ(alpha(x,0),0); EXPECT_EQ(alpha(x,image.height-1),0);
        }
        for (uint32_t y = 0; y < image.height; ++y) {
            EXPECT_EQ(alpha(0,y),0); EXPECT_EQ(alpha(image.width-1,y),0);
        }
        for (auto vertex : {cgl::MapPoint{image.width*0.5,image.height*0.5-23.5*scale},
                {image.width*0.5+32*scale,image.height*0.5},
                {image.width*0.5,image.height*0.5+23.5*scale},
                {image.width*0.5-32*scale,image.height*0.5}}) {
            bool visible = false;
            for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
                visible |= alpha(uint32_t(vertex.x)+dx,uint32_t(vertex.y)+dy) != 0;
            EXPECT_TRUE(visible);
        }
    }
}

TEST(MapHoverTest, InvalidOutlineScaleIsRejectedBeforeAllocation) {
    for (double scale : {0.0,-1.0,17.0,std::numeric_limits<double>::quiet_NaN()})
        EXPECT_THROW(cgl::BuildMapHoverBitmap(scale),std::invalid_argument);
}
}

TEST(MapHoverInput, ResolvesHiDpiViewportWithoutChangingInputOrCamera) {
    const auto layout=Ground(3,2);
    const cgl::component::MapCamera camera{{1,1},1};
    const cgl::Viewport viewport{17,23,400,300};
    cgl::MapPointerInput input{{108.5,86.5},{400,300},{800,600},true};
    EXPECT_EQ(cgl::ResolveMapHover(input,camera,viewport,layout),(cgl::MapCellCoord{1,1}));
    EXPECT_DOUBLE_EQ(input.cursor.x,108.5);EXPECT_DOUBLE_EQ(camera.center.x,1);
    input.inside=false;EXPECT_FALSE(cgl::ResolveMapHover(input,camera,viewport,layout));
    input.inside=true;input.cursor={0,0};EXPECT_FALSE(cgl::ResolveMapHover(input,camera,viewport,layout));
}
