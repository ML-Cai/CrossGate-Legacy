#pragma once
#include "cgl/graphics/sprite_region.h"

namespace cgl {
struct MapImage {
    graphics::SpriteRegion source;
    int32_t offsetX, offsetY;
    uint8_t tileX = 1, tileY = 1;
    uint32_t atlasIndex = 0;
};

// Classic map placeholders/debug markers are not scene artwork.
inline bool IsClassicMapImage(uint16_t id, bool object) noexcept {
    return id != 0 && id != 2 && (!object || id < 200 || id > 265);
}
}
