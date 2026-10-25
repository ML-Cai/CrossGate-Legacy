#pragma once
#include <unordered_map>
#include "cgl/assets/map_layout_reader.h"
#include "map/map_image.h"

namespace cgl {
// Owned CPU data. No reader pointers, window or GPU objects.
struct MapData {
    MapLayout layout;
    uint64_t rgbaBytes = 0; // Planned atlas payload, not allocated CPU pixels.
    std::unordered_map<uint16_t, MapImage> images;
    double reachX = 0, reachY = 0;
};

}
