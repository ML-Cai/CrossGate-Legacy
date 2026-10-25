#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "cgl/graphics/device.h"
#include "assets/map_builder.h"
#include "map/map_hover.h"
#include "cgl/trace/logger.h"
#include <stdexcept>

namespace cgl {
std::shared_ptr<const MapHoverOutline> MapLibrary::acquireHoverOutline(double scale,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass) ||
        !std::isfinite(scale) || scale < 0.125 || scale > 16)
        throw std::invalid_argument("Hover outline requires a valid scale and one live device/render pass");
    if(auto found=outlines_.find(scale)) return found;
    std::shared_ptr<const MapHoverOutline> restored;
    if(retiredOutlines_.restore(scale,[&](const auto& outline,uint64_t bytes) {
        outlines_.insert(scale,outline,bytes);restored=outline;
    })) return restored;
    const auto image = BuildMapHoverBitmap(scale);
    auto result = std::make_shared<MapHoverOutline>();
    result->renderer = graphics::ISpriteRenderer::create(device, pass, image.width, image.height, 1, image.rgba);
    if (!result->renderer) throw std::runtime_error("Could not upload map hover outline");
    result->width = image.width; result->height = image.height; result->scale = scale;
    outlines_.insert(scale,result,uint64_t(result->width)*result->height*4);
    device_ = device; pass_ = pass;
    return result;
}

std::shared_ptr<const MapResources> MapLibrary::acquire(const std::filesystem::path& path,
    component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Map library requires one live device/render pass");
    const auto key = std::filesystem::absolute(path).lexically_normal();
    if(auto found=maps_.find(key)) return found;
    std::shared_ptr<const MapResources> restored;
    if(retiredMaps_.restore(key,[&](const auto& map,uint64_t bytes) {
        maps_.insert(key,map,bytes);restored=map;
    })) return restored;
    constexpr auto version = CrossGateVersion::CG_VERSION_Classic;
    auto prepared = [&] {
        std::lock_guard readerLock(*readers.queryMutex);
        MapLayout layout;
        if (IMapLayoutReader::create()->read(key, &layout) != Results::Success)
            throw std::runtime_error("Could not read MAP layout: " + key.string());
        if (!readers.pGfxInfoReader[version] || !readers.pGfxDataReader[version] || !readers.pPaletteReader)
            throw std::runtime_error("Map requires initialized Classic readers");
        return MapBuilder(std::move(layout), *readers.pGfxInfoReader[version], *readers.pPaletteReader);
    }();
    auto result = std::make_shared<MapResources>();
    for (size_t page = 0; page < prepared.pageCount(); ++page) {
        // Hold only one page of CPU pixels. GPU creation is owner-thread work,
        // outside the reader lock so animation CPU preparation can make progress.
        auto pixels = [&] {
            std::lock_guard readerLock(*readers.queryMutex);
            return prepared.buildPage(page, *readers.pGfxDataReader[version]);
        }();
        auto renderer = graphics::ISpriteRenderer::create(device, pass, pixels.width, pixels.height, 1, pixels.rgba);
        if (!renderer) throw std::runtime_error("Could not upload map atlas page (check device texture limits)");
        result->renderers.push_back(std::move(renderer));
    }
    auto data = std::move(prepared).takeData();
    result->layout = std::move(data.layout);
    result->images = std::move(data.images);
    result->rgbaBytes = data.rgbaBytes;
    result->reachX = data.reachX; result->reachY = data.reachY;
    maps_.insert(key,result,result->rgbaBytes);
    device_ = device; pass_ = pass;
    LOGI("Map ready: " << result->layout.width << "x" << result->layout.height
        << ", images=" << result->images.size() << ", atlas pages=" << result->renderers.size()
        << ", texture bytes=" << result->rgbaBytes);
    return result;
}
uint64_t MapLibrary::retireUnused(uint64_t mapTargetBytes,uint64_t lastSubmitted,uint64_t outlineTargetBytes) {
    const auto maps=maps_.retireKeyed(mapTargetBytes,[]{},[&](const auto& key,const auto& map,uint64_t bytes) {
        retiredMaps_.retain(key,lastSubmitted,map,bytes);
    },mapTargetBytes?2:0);
    return maps+outlines_.retireKeyed(outlineTargetBytes,[]{},[&](const auto& key,const auto& outline,uint64_t bytes) {
        retiredOutlines_.retain(key,lastSubmitted,outline,bytes);
    },outlineTargetBytes?16:0);
}
uint64_t MapLibrary::trimUnused(uint64_t mapTargetBytes,uint64_t outlineTargetBytes) {
    bool synchronized=false;
    auto barrier=[&] {
        if(synchronized) return;
        if(!device_ || device_->waitIdle()!=Results::Success)
            throw std::runtime_error("Cannot synchronize map cache eviction");
        synchronized=true;
    };
    auto bytes=maps_.trim(mapTargetBytes,barrier,mapTargetBytes?2:0);
    return bytes+outlines_.trim(outlineTargetBytes,barrier,outlineTargetBytes?16:0);
}

}
