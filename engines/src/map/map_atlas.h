#pragma once

#include <algorithm>
#include <optional>
#include <vector>
#include <unordered_set>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
struct MapImageSize { uint16_t id; uint32_t width, height; };
struct MapAtlasRegion { uint16_t id; graphics::SpriteRegion source; };
struct MapAtlasPlan {
    uint32_t width = 0, height = 0;
    std::vector<MapAtlasRegion> regions;
};

// Deterministic shelves, with a transparent one-pixel border per image.
// Plan every page before decoding. At most 8 pages, each <=32 MiB RGBA;
// GPU/staging overhead is additional. Image IDs must be unique across pages.
inline std::optional<std::vector<MapAtlasPlan>> PlanMapAtlases(std::vector<MapImageSize> images,
    uint32_t maxWidth = 2048, uint32_t maxHeight = 4096, uint32_t maxPages = 8) {
    if (maxWidth < 3 || maxHeight < 3 || maxWidth > 2048 || maxHeight > 4096 ||
        !maxPages || maxPages > 8) return std::nullopt;
    std::sort(images.begin(), images.end(), [](const auto& a, const auto& b) {
        if (a.height != b.height) return a.height > b.height;
        if (a.width != b.width) return a.width > b.width;
        return a.id < b.id;
    });
    std::unordered_set<uint16_t> ids;
    std::vector<MapAtlasPlan> pages;
    uint32_t x = 1, y = 1, rowHeight = 0;
    for (const auto& image : images) {
        if (!image.width || !image.height || image.width > maxWidth - 2 || image.height > maxHeight - 2 ||
            !ids.insert(image.id).second) return std::nullopt;
        if (pages.empty()) pages.emplace_back();
        if (x + image.width + 1 > maxWidth) { x = 1; y += rowHeight + 2; rowHeight = 0; }
        if (y + image.height + 1 > maxHeight) {
            if (pages.size() == maxPages) return std::nullopt;
            pages.emplace_back();
            x = y = 1; rowHeight = 0;
        }
        auto& plan = pages.back();
        plan.regions.push_back({image.id, {x, y, image.width, image.height}});
        plan.width = std::max(plan.width, x + image.width + 1);
        plan.height = std::max(plan.height, y + image.height + 1);
        x += image.width + 2;
        rowHeight = std::max(rowHeight, image.height);
    }
    return pages;
}

inline std::optional<MapAtlasPlan> PlanMapAtlas(std::vector<MapImageSize> images,
    uint32_t maxWidth = 2048, uint32_t maxHeight = 4096) {
    auto pages = PlanMapAtlases(std::move(images), maxWidth, maxHeight, 1);
    if (!pages) return std::nullopt;
    return pages->empty() ? MapAtlasPlan{} : std::move(pages->front());
}
}
