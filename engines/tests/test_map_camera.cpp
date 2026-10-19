#include "map/map_camera.h"
#include <gtest/gtest.h>

namespace {
TEST(MapCameraTest, ProjectionRoundTripsAtDifferentCentersAndScales) {
    const cgl::Viewport viewport{17, 23, 800, 600};
    for (double scale : {0.125, 1.0, 2.0, 16.0}) {
        const cgl::component::MapCamera camera{{17.25, 35.5}, scale};
        for (cgl::MapPoint cell : {cgl::MapPoint{0, 0}, {10, 43}, {99, 99}, {-2.5, 7.125}}) {
            const auto screen = cgl::MapToScreen(cell, camera, viewport);
            const auto restored = cgl::ScreenToMap(screen, camera, viewport);
            EXPECT_NEAR(restored.x, cell.x, 1e-9);
            EXPECT_NEAR(restored.y, cell.y, 1e-9);
        }
        const auto center = cgl::MapToScreen(camera.center, camera, viewport);
        EXPECT_DOUBLE_EQ(center.x, 400);
        EXPECT_DOUBLE_EQ(center.y, 300);
    }
    const auto east = cgl::MapToWorld({1, 0});
    EXPECT_DOUBLE_EQ(east.x, 32);
    EXPECT_DOUBLE_EQ(east.y, 23.5);
    const auto south = cgl::MapToWorld({0, 1});
    EXPECT_DOUBLE_EQ(south.x, -32);
    EXPECT_DOUBLE_EQ(south.y, 23.5);
}

TEST(MapCameraTest, PanUsesElapsedTimeZoomAndClampsAtMapEdges) {
    cgl::component::MapCamera camera{{50, 50}, 2};
    const auto before = cgl::MapToWorld(camera.center);
    cgl::PanMapCamera(camera, 1, 1, 0.5, 100, 100);
    const auto after = cgl::MapToWorld(camera.center);
    EXPECT_NEAR(std::hypot(after.x - before.x, after.y - before.y), 75, 1e-9);
    EXPECT_NEAR(after.x - before.x, after.y - before.y, 1e-9);
    auto split = cgl::component::MapCamera{{50, 50}, 2};
    cgl::PanMapCamera(split, 1, 1, 0.25, 100, 100);
    cgl::PanMapCamera(split, 1, 1, 0.25, 100, 100);
    EXPECT_NEAR(split.center.x, camera.center.x, 1e-9);
    EXPECT_NEAR(split.center.y, camera.center.y, 1e-9);
    cgl::PanMapCamera(camera, 0, 1, 1000, 100, 60);
    EXPECT_DOUBLE_EQ(camera.center.x, 99);
    EXPECT_DOUBLE_EQ(camera.center.y, 59);
    cgl::PanMapCamera(camera, 0, -1, 1000, 100, 60);
    EXPECT_DOUBLE_EQ(camera.center.x, 0);
    EXPECT_DOUBLE_EQ(camera.center.y, 0);
    cgl::PanMapCamera(camera, 1, 0, -1, 100, 60);
    EXPECT_DOUBLE_EQ(camera.center.x, 0);
}

TEST(MapCameraTest, CullingIncludesEveryIntersectingTileAcrossEdgesAndZoom) {
    const cgl::Viewport viewport{0, 0, 800, 600};
    for (double scale : {0.125, 1.0, 2.0, 16.0}) {
        for (const auto center : {cgl::MapPoint{0, 0}, {49.5, 24.5}, {99, 49}}) {
            const cgl::component::MapCamera camera{center, scale};
            // Include nonstandard offsets to verify that culling uses image reach.
            const auto bounds = cgl::VisibleMapBounds(camera, viewport, 100, 50, 160, 120);
            for (int y = 0; y < 50; ++y) for (int x = 0; x < 100; ++x) {
                const auto p = cgl::MapToScreen({double(x), double(y)}, camera, viewport);
                if (p.x - 160 * scale < 800 && p.x + 160 * scale > 0 &&
                    p.y - 120 * scale < 600 && p.y + 120 * scale > 0) {
                    EXPECT_GE(x, bounds.left); EXPECT_LE(x, bounds.right);
                    EXPECT_GE(y, bounds.top); EXPECT_LE(y, bounds.bottom);
                }
            }
        }
    }
    EXPECT_LT(cgl::VisibleMapBounds({}, viewport, 0, 0, 32, 24).right, 0);
}
}
