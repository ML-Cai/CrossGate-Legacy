#pragma once

#include <optional>
#include <stdexcept>
#include <vector>
#include "map/map_render_data.h"
#include "cgl/assets/map_layout_reader.h"

namespace cgl {
struct MapCellCoord {
    int32_t x = 0, y = 0;
    bool operator==(const MapCellCoord&) const = default;
};
namespace component {
struct MapHover { std::optional<MapCellCoord> cell; };
}

// GLFW cursor/window sizes are logical units. Convert to the render surface,
// then to viewport-local pixels, matching MapToScreen and ISpriteRenderer.
inline std::optional<MapPoint> WindowToViewport(MapPoint cursor, MapPoint windowSize,
                                               MapPoint renderSize, const Viewport& viewport) noexcept {
    if (!std::isfinite(cursor.x) || !std::isfinite(cursor.y) ||
        !std::isfinite(windowSize.x) || !std::isfinite(windowSize.y) ||
        !std::isfinite(renderSize.x) || !std::isfinite(renderSize.y) ||
        windowSize.x <= 0 || windowSize.y <= 0 || renderSize.x <= 0 || renderSize.y <= 0 ||
        !viewport.width || !viewport.height || viewport.x < 0 || viewport.y < 0 ||
        double(viewport.x) + viewport.width > renderSize.x ||
        double(viewport.y) + viewport.height > renderSize.y ||
        cursor.x < 0 || cursor.y < 0 || cursor.x >= windowSize.x || cursor.y >= windowSize.y)
        return std::nullopt;
    MapPoint point{cursor.x / windowSize.x * renderSize.x - viewport.x,
                   cursor.y / windowSize.y * renderSize.y - viewport.y};
    if (point.x < 0 || point.y < 0 || point.x >= viewport.width || point.y >= viewport.height)
        return std::nullopt;
    return point;
}

inline std::optional<MapCellCoord> PickMapCell(MapPoint screen, const component::MapCamera& camera,
                                              const Viewport& viewport, const MapLayout& layout) noexcept {
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
        !std::isfinite(camera.center.x) || !std::isfinite(camera.center.y) ||
        !std::isfinite(camera.scale) || camera.scale <= 0 || !viewport.width || !viewport.height ||
        screen.x < 0 || screen.y < 0 || screen.x >= viewport.width || screen.y >= viewport.height)
        return std::nullopt;
    const auto point = ScreenToMap(screen, camera, viewport);
    // A cell owns [n-0.5, n+0.5) along each map axis. Exact ties go to the larger
    // coordinate, including negative edges; truncation toward zero is incorrect.
    const double x = std::floor(point.x + 0.5), y = std::floor(point.y + 0.5);
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x >= layout.width || y >= layout.height)
        return std::nullopt;
    const auto* cell = layout.cell(int32_t(x), int32_t(y));
    if (!cell || !IsClassicMapImage(cell->groundId, false)) return std::nullopt;
    return MapCellCoord{int32_t(x), int32_t(y)};
}

struct MapHoverBitmap {
    uint32_t width, height;
    std::vector<uint8_t> rgba;
};

// Rasterize once at the configured scale so even the minimum zoom has a visible
// two-pixel outline. Transparent center; no dependency on original game artwork.
inline MapHoverBitmap BuildMapHoverBitmap(double scale) {
    if (!std::isfinite(scale) || scale < 0.125 || scale > 16)
        throw std::invalid_argument("Hover scale must be between 0.125 and 16");
    const double halfWidth = 32 * scale, halfHeight = 23.5 * scale;
    MapHoverBitmap image{uint32_t(std::ceil(64 * scale)) + 6, uint32_t(std::ceil(47 * scale)) + 6, {}};
    image.rgba.resize(size_t(image.width) * image.height * 4, 0);
    const double normalLength = std::hypot(1 / halfWidth, 1 / halfHeight);
    for (uint32_t y = 0; y < image.height; ++y) for (uint32_t x = 0; x < image.width; ++x) {
        const double edge = std::abs(x + 0.5 - image.width * 0.5) / halfWidth +
            std::abs(y + 0.5 - image.height * 0.5) / halfHeight;
        if (std::abs(edge - 1) / normalLength <= 1) {
            auto* pixel = image.rgba.data() + (size_t(y) * image.width + x) * 4;
            pixel[0] = 255; pixel[1] = 230; pixel[2] = 40; pixel[3] = 255;
        }
    }
    return image;
}
}
