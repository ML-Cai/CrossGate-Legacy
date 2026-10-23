#pragma once

#include <filesystem>
#include "assets/idle_resource_cache.h"
#include "assets/keyed_retirement_cache.h"
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
// before destroying the render pass/device. trimUnused waits before releasing idle uploads.
class MapLibrary {
public:
    std::shared_ptr<const MapResources> acquire(const std::filesystem::path& path,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    std::shared_ptr<const MapHoverOutline> acquireHoverOutline(double scale,
        graphics::IDevice* device, graphics::IRenderPass* pass);
    struct Usage {size_t maps=0,pages=0,outlines=0;uint64_t rgbaBytes=0,outlineBytes=0,evictions=0,retiredMapBytes=0,retiredOutlineBytes=0;size_t retiredMaps=0,retiredOutlines=0;uint64_t reusedMaps=0,reusedOutlines=0;};
    Usage usage() const noexcept {
        Usage result{maps_.size(),0,outlines_.size(),maps_.bytes(),outlines_.bytes(),maps_.evictions()+outlines_.evictions(),retiredMaps_.bytes(),retiredOutlines_.bytes(),retiredMaps_.size(),retiredOutlines_.size(),retiredMaps_.restores(),retiredOutlines_.restores()};
        maps_.forEach([&](const MapResources& map){result.pages+=map.renderers.size();});
        return result;
    }
    uint64_t trimUnused(uint64_t mapTargetBytes,uint64_t outlineTargetBytes=1024*1024);
    // Internal owner-thread API: serials must come from this device's single
    // graphics submission tracker. Retired resources still own their allocations.
    uint64_t retireUnused(uint64_t mapTargetBytes,uint64_t lastSubmitted,uint64_t outlineTargetBytes=1024*1024);
    uint64_t collectRetired(uint64_t completed,size_t maxMaps=std::numeric_limits<size_t>::max(),
            size_t maxOutlines=std::numeric_limits<size_t>::max()) noexcept {
        return retiredMaps_.collect(completed,maxMaps)+retiredOutlines_.collect(completed,maxOutlines);
    }
    void clear() { retiredMaps_.clear();retiredOutlines_.clear(); maps_.clear(); outlines_.clear(); device_ = nullptr; pass_ = nullptr; }
private:
    IdleResourceCache<std::filesystem::path,MapResources> maps_;
    IdleResourceCache<double,MapHoverOutline> outlines_;
    KeyedRetirementCache<std::filesystem::path,MapResources> retiredMaps_;
    KeyedRetirementCache<double,MapHoverOutline> retiredOutlines_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
