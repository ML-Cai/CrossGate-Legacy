#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "cgl/graphics/device.h"
#include "map/map_atlas.h"
#include "map/map_render_data.h"
#include "map/map_hover.h"
#include "cgl/trace/logger.h"
#include <set>
#include <stdexcept>

namespace cgl {
std::shared_ptr<const MapHoverOutline> MapLibrary::acquireHoverOutline(double scale,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass) ||
        !std::isfinite(scale) || scale < 0.125 || scale > 16)
        throw std::invalid_argument("Hover outline requires a valid scale and one live device/render pass");
    if(auto found=outlines_.find(scale)) return found;
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
    std::lock_guard readerLock(*readers.queryMutex);
    auto result = std::make_shared<MapResources>();
    if (IMapLayoutReader::create()->read(key, &result->layout) != Results::Success)
        throw std::runtime_error("Could not read MAP layout: " + key.string());
    constexpr auto version = CrossGateVersion::CG_VERSION_Classic;
    auto* infoReader = readers.pGfxInfoReader[version].get();
    auto* dataReader = readers.pGfxDataReader[version].get();
    if (!infoReader || !dataReader || !readers.pPaletteReader)
        throw std::runtime_error("Map requires initialized Classic readers");
    PaletteData256 palette;
    if (readers.pPaletteReader->read(EnvironmentPaletteTypes::Daytime, &palette) != Results::Success)
        throw std::runtime_error("Could not read map palette");
    std::set<uint16_t> ids;
    for (const auto& cell : result->layout.cells) {
        if (IsClassicMapImage(cell.groundId, false)) ids.insert(cell.groundId);
        if (IsClassicMapImage(cell.objectId, true)) ids.insert(cell.objectId);
    }
    std::unordered_map<uint16_t, GraphicsResourceInfo> infos;
    std::vector<MapImageSize> sizes;
    for (const auto id : ids) {
        GraphicsResourceInfo info{};
        if (infoReader->query({GraphicsResourceSerialNumTypes::MapSerialNum, version, id}, &info) != Results::Success)
            throw std::runtime_error("Classic map image ID not found: " + std::to_string(id));
        if (info.width <= 0 || info.height <= 0)
            throw std::runtime_error("Invalid map image dimensions: " + std::to_string(id));
        sizes.push_back({id, uint32_t(info.width), uint32_t(info.height)});
        infos.emplace(id, info);
    }
    const auto plans = PlanMapAtlases(std::move(sizes));
    if (!plans) throw std::runtime_error("Map images exceed 8 atlas pages of 2048x4096 or contain an oversized image");
    uint64_t textureBytes = 0;
    for (const auto& plan : *plans) {
        textureBytes += uint64_t(plan.width) * plan.height * 4;
        const auto atlasIndex = uint32_t(result->renderers.size());
        std::vector<uint8_t> rgba(size_t(plan.width) * plan.height * 4, 0);
        for (const auto& region : plan.regions) {
            const auto& info = infos.at(region.id);
            GraphicsResourceData data{};
            if (dataReader->query(info, &data) != Results::Success || !data.pData ||
                data.width != info.width || data.height != info.height)
                throw std::runtime_error("Could not decode map image: " + std::to_string(region.id));
            const auto* colors = data.pPaletteData ? data.pPaletteData :
                reinterpret_cast<const uint8_t*>(palette.data());
            for (int y = 0; y < data.height; ++y) for (int x = 0; x < data.width; ++x) {
                const auto index = data.pData[(data.height - 1 - y) * size_t(data.width) + x];
                auto* dst = rgba.data() + ((size_t(region.source.y) + y) * plan.width + region.source.x + x) * 4;
                dst[0] = colors[index * 3 + 2]; dst[1] = colors[index * 3 + 1];
                dst[2] = colors[index * 3]; dst[3] = index ? 255 : 0;
            }
            result->images.emplace(region.id, MapImage{region.source, info.offsetX, info.offsetY,
                std::max(uint8_t(1), info.tileX), std::max(uint8_t(1), info.tileY), atlasIndex});
            result->reachX = std::max({result->reachX, std::abs(double(info.offsetX)),
                std::abs(double(info.offsetX) + info.width)});
            result->reachY = std::max({result->reachY, std::abs(double(info.offsetY)),
                std::abs(double(info.offsetY) + info.height)});
        }
        auto renderer = graphics::ISpriteRenderer::create(device, pass, plan.width, plan.height, 1, rgba);
        if (!renderer) throw std::runtime_error("Could not upload map atlas page (check device texture limits)");
        result->renderers.push_back(std::move(renderer));
    }
    result->rgbaBytes=textureBytes;
    maps_.insert(key,result,result->rgbaBytes);
    device_ = device; pass_ = pass;
    LOGI("Map ready: " << result->layout.width << "x" << result->layout.height
        << ", images=" << result->images.size() << ", atlas pages=" << result->renderers.size()
        << ", texture bytes=" << textureBytes);
    return result;
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
