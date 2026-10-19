#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "cgl/common/viewport.h"

namespace cgl {
struct MapPoint { double x = 0, y = 0; };

// Classic ground diamond size. Map x/y are cell coordinates; screen y goes down.
inline MapPoint MapToWorld(MapPoint cell) noexcept {
    return {(cell.x - cell.y) * 32.0, (cell.x + cell.y) * 23.5};
}
inline MapPoint WorldToMap(MapPoint point) noexcept {
    return {point.x / 64.0 + point.y / 47.0,
            point.y / 47.0 - point.x / 64.0};
}

namespace component {
struct MapCamera {
    MapPoint center; // In cell coordinates, including fractional movement.
    double scale = 1;
};
}

inline MapPoint MapToScreen(MapPoint cell, const component::MapCamera& camera,
                            const Viewport& viewport) noexcept {
    const auto p = MapToWorld(cell);
    const auto center = MapToWorld(camera.center);
    return {(p.x - center.x) * camera.scale + viewport.width * 0.5,
            (p.y - center.y) * camera.scale + viewport.height * 0.5};
}

// Screen coordinates are relative to the viewport's top-left, as in ISpriteRenderer.
inline MapPoint ScreenToMap(MapPoint screen, const component::MapCamera& camera,
                            const Viewport& viewport) noexcept {
    const auto center = MapToWorld(camera.center);
    return WorldToMap({(screen.x - viewport.width * 0.5) / camera.scale + center.x,
                       (screen.y - viewport.height * 0.5) / camera.scale + center.y});
}

inline void PanMapCamera(component::MapCamera& camera, double horizontal, double vertical,
                         double seconds, uint32_t width, uint32_t height) noexcept {
    if (!width || !height || !std::isfinite(seconds) || seconds <= 0) return;
    const double length = std::hypot(horizontal, vertical);
    if (!length) return;
    // 300 screen pixels per second, independent of diagonal movement and zoom.
    const auto delta = WorldToMap({horizontal / length * 300 * seconds / camera.scale,
                                   vertical / length * 300 * seconds / camera.scale});
    camera.center.x = std::clamp(camera.center.x + delta.x, 0.0, double(width - 1));
    camera.center.y = std::clamp(camera.center.y + delta.y, 0.0, double(height - 1));
}

struct MapBounds { int32_t left = 0, top = 0, right = -1, bottom = -1; };

inline MapBounds VisibleMapBounds(const component::MapCamera& camera, const Viewport& viewport,
                                  uint32_t width, uint32_t height, double reachX, double reachY) noexcept {
    if (!width || !height || !viewport.width || !viewport.height) return {};
    const double x = reachX * camera.scale, y = reachY * camera.scale;
    const std::array<MapPoint, 4> corners{{{-x, -y}, {viewport.width + x, -y},
        {-x, viewport.height + y}, {viewport.width + x, viewport.height + y}}};
    double left = width, right = -1, top = height, bottom = -1;
    for (const auto& corner : corners) {
        const auto p = ScreenToMap(corner, camera, viewport);
        left = std::min(left, p.x); right = std::max(right, p.x);
        top = std::min(top, p.y); bottom = std::max(bottom, p.y);
    }
    return {int32_t(std::clamp(std::floor(left), 0.0, double(width - 1))),
            int32_t(std::clamp(std::floor(top), 0.0, double(height - 1))),
            int32_t(std::clamp(std::ceil(right), 0.0, double(width - 1))),
            int32_t(std::clamp(std::ceil(bottom), 0.0, double(height - 1)))};
}
}
