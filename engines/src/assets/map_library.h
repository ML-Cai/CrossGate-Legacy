#pragma once

#include <filesystem>
#include "assets/idle_resource_cache.h"
#include "assets/keyed_retirement_cache.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include "cgl/assets/map_layout_reader.h"
#include "cgl/graphics/sprite_renderer.h"
#include "map/map_data.h"
#include "assets/map_decode_worker.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
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
    ~MapLibrary() {cancelPending();}
    std::shared_ptr<const MapResources> acquire(const std::filesystem::path& path,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    // One map, one CPU slot, one uploading page. Poll once per frame on the
    // graphics owner thread; each call submits at most one <=4 MiB copy chunk.
    // Different misses require cancelPending first. Readers/device/pass outlive it.
    std::shared_ptr<const MapResources> acquireProgressive(const std::filesystem::path& path,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    struct Progress {
        bool loading=false,cpuPending=false,uploadPending=false;
        size_t completed=0,total=0;
        uint64_t workingBytes=0,pendingBytes=0;
    };
    Progress progress() const;
    // Blocking exit/shutdown boundary: join CPU access before releasing its
    // borrowed builder/readers, then wait only for the submitted upload chunk.
    void cancelPending() noexcept;
    std::shared_ptr<const MapHoverOutline> acquireHoverOutline(double scale,
        graphics::IDevice* device, graphics::IRenderPass* pass);
    struct Usage {
        size_t maps=0,pages=0,outlines=0;
        uint64_t rgbaBytes=0,outlineBytes=0,evictions=0,retiredMapBytes=0,retiredOutlineBytes=0;
        size_t retiredMaps=0,retiredOutlines=0;
        uint64_t reusedMaps=0,reusedOutlines=0,pendingBytes=0,workingBytes=0,peakWorkingBytes=0,peakPendingBytes=0;
        bool loading=false,cpuPending=false,uploadPending=false;
        size_t completedPages=0,totalPages=0;
        double maxOwnerCallMs=0;
    };
    Usage usage() const noexcept {
        Usage result{maps_.size(),0,outlines_.size(),maps_.bytes(),outlines_.bytes(),maps_.evictions()+outlines_.evictions(),retiredMaps_.bytes(),retiredOutlines_.bytes(),retiredMaps_.size(),retiredOutlines_.size(),retiredMaps_.restores(),retiredOutlines_.restores()};
        maps_.forEach([&](const MapResources& map){result.pages+=map.renderers.size();});
        const auto p=progress();result.pendingBytes=p.pendingBytes;result.workingBytes=p.workingBytes;
        result.maxOwnerCallMs=maxOwnerCallMs_;
        result.peakWorkingBytes=peakWorkingBytes_;result.peakPendingBytes=peakPendingBytes_;
        result.loading=p.loading;result.cpuPending=p.cpuPending;result.uploadPending=p.uploadPending;
        result.completedPages=p.completed;result.totalPages=p.total;
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
    void clear() {
        cancelPending();decoder_.stop();
        retiredMaps_.clear();retiredOutlines_.clear();maps_.clear();outlines_.clear();
        peakWorkingBytes_=peakPendingBytes_=0;maxOwnerCallMs_=0;device_=nullptr;pass_=nullptr;
    }
private:
    struct PendingMap {
        std::filesystem::path key;
        std::unique_ptr<MapBuilder> plan;
        std::shared_ptr<MapResources> result=std::make_shared<MapResources>();
        graphics::ISpriteRenderer::Ptr upload;
        uint64_t uploadBytes=0;
        size_t page=0;
    };
    std::unique_ptr<PendingMap> pending_;
    MapDecodeWorker decoder_;
    uint64_t peakWorkingBytes_=0,peakPendingBytes_=0;
    double maxOwnerCallMs_=0;
    IdleResourceCache<std::filesystem::path,MapResources> maps_;
    IdleResourceCache<double,MapHoverOutline> outlines_;
    KeyedRetirementCache<std::filesystem::path,MapResources> retiredMaps_;
    KeyedRetirementCache<double,MapHoverOutline> retiredOutlines_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
