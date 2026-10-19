#pragma once

#include <algorithm>
#include <optional>
#include <vector>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
struct MapImageSize { uint16_t id; uint32_t width, height; };
struct MapAtlasRegion { uint16_t id; graphics::SpriteRegion source; };
struct MapAtlasPlan {
    uint32_t width = 0, height = 0;
    std::vector<MapAtlasRegion> regions;
};

// Deterministic shelves with a transparent one-pixel border around each image.
// Bound the atlas before decoding or allocating RGBA pixels (at most 32 MiB).
inline std::optional<MapAtlasPlan> PlanMapAtlas(std::vector<MapImageSize> images,
                                               uint32_t maxWidth = 2048, uint32_t maxHeight = 4096) {
    if (maxWidth < 3 || maxHeight < 3 || maxWidth > 2048 || maxHeight > 4096) return std::nullopt;
    std::sort(images.begin(), images.end(), [](const auto& a, const auto& b) {
        if (a.height != b.height) return a.height > b.height;
        if (a.width != b.width) return a.width > b.width;
        return a.id < b.id;
    });
    MapAtlasPlan plan;
    uint32_t x = 1, y = 1, rowHeight = 0;
    for (const auto& image : images) {
        if (!image.width || !image.height || image.width > maxWidth - 2 || image.height > maxHeight - 2)
            return std::nullopt;
        if (x + image.width + 1 > maxWidth) { x = 1; y += rowHeight + 2; rowHeight = 0; }
        if (y + image.height + 1 > maxHeight) return std::nullopt;
        plan.regions.push_back({image.id, {x, y, image.width, image.height}});
        plan.width = std::max(plan.width, x + image.width + 1);
        plan.height = std::max(plan.height, y + image.height + 1);
        x += image.width + 2;
        rowHeight = std::max(rowHeight, image.height);
    }
    return plan;
}
}
