#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <unordered_map>
#include "cgl/assets/map_layout_reader.h"
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
struct MapImage {
    graphics::SpriteRegion source;
    int32_t offsetX, offsetY;
    uint8_t tileX = 1, tileY = 1;
};
struct MapResources {
    MapLayout layout;
    std::unordered_map<uint16_t, MapImage> images;
    graphics::ISpriteRenderer::Ptr renderer;
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
    void clear() { maps_.clear(); outlines_.clear(); device_ = nullptr; pass_ = nullptr; }
private:
    std::map<std::filesystem::path, std::shared_ptr<const MapResources>> maps_;
    std::map<double, std::shared_ptr<const MapHoverOutline>> outlines_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
