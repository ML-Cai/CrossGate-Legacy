#pragma once
#include <cstdint>

namespace cgl::graphics {
// Pixel rectangle only; usable without a renderer or device.
struct SpriteRegion {
    uint32_t x = 0, y = 0, width = 0, height = 0;
};
}
