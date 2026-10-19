#pragma once

#include "map/map_camera.h"
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
// Classic map placeholders/debug markers are not scene artwork.
inline bool IsClassicMapImage(uint16_t id, bool object) noexcept {
    return id != 0 && id != 2 && (!object || id < 200 || id > 265);
}
namespace component {
struct MapPosition { MapPoint cell; };
}

struct MapDrawItem {
    MapPoint cell;
    graphics::ISpriteRenderer* renderer = nullptr;
    graphics::SpriteRegion source;
    uint32_t frame = 0;
    int32_t offsetX = 0, offsetY = 0;
    uint64_t stableId = 0;
};

// Paint back to front by ground anchor, never by the top of the image. Break
// equal-depth ties left-to-right and then by a stable object/entity identity.
inline bool MapDrawLess(const MapDrawItem& a, const MapDrawItem& b) noexcept {
    const double depthA = a.cell.y - a.cell.x, depthB = b.cell.y - b.cell.x;
    if (depthA != depthB) return depthA < depthB;
    const double sideA = a.cell.x + a.cell.y, sideB = b.cell.x + b.cell.y;
    if (sideA != sideB) return sideA < sideB;
    return a.stableId < b.stableId;
}

inline bool MapImageVisible(MapPoint position, uint32_t width, uint32_t height,
                             double scale, const Viewport& viewport) noexcept {
    return position.x < viewport.width && position.y < viewport.height &&
        position.x + width * scale > 0 && position.y + height * scale > 0;
}
}
