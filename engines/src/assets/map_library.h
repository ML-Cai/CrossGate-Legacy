#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include "cgl/assets/map_layout_reader.h"
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
struct MapImage {
    graphics::SpriteRegion source;
    int32_t offsetX, offsetY;
    uint8_t tileX = 1, tileY = 1;
    uint32_t atlasIndex = 0;
};
struct MapResources {
    MapLayout layout;
    uint64_t rgbaBytes=0;
    std::unordered_map<uint16_t, MapImage> images;
    std::vector<graphics::ISpriteRenderer::Ptr> renderers;
    double reachX = 0, reachY = 0;
};

struct MapHoverOutline {
    graphics::ISpriteRenderer::Ptr renderer;
    uint32_t width = 0, height = 0;
    double scale = 1;
};

// Engine-owned static map cache. Clear after GPU completion and scene exit,
// before destroying the render pass/device. Scene changes never evict uploads.
class MapLibrary {
public:
    std::shared_ptr<const MapResources> acquire(const std::filesystem::path& path,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    std::shared_ptr<const MapHoverOutline> acquireHoverOutline(double scale,
        graphics::IDevice* device, graphics::IRenderPass* pass);
    struct Usage {size_t maps=0,pages=0,outlines=0;uint64_t rgbaBytes=0,outlineBytes=0;};
    Usage usage() const noexcept {
        Usage result{maps_.size(),0,outlines_.size(),0,0};
        for(const auto& [key,map]:maps_) {result.pages+=map->renderers.size();result.rgbaBytes+=map->rgbaBytes;}
        for(const auto& [key,outline]:outlines_) result.outlineBytes+=uint64_t(outline->width)*outline->height*4;
        return result;
    }
    void clear() { maps_.clear(); outlines_.clear(); device_ = nullptr; pass_ = nullptr; }
private:
    std::map<std::filesystem::path, std::shared_ptr<const MapResources>> maps_;
    std::map<double, std::shared_ptr<const MapHoverOutline>> outlines_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
