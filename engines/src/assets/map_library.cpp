#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "cgl/graphics/device.h"
#include "assets/map_builder.h"
#include "map/map_hover.h"
#include "cgl/trace/logger.h"
#include <stdexcept>
#include <chrono>
#include <thread>

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
    if(pending_) throw std::logic_error("Cancel progressive map loading before a synchronous cache miss");
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
namespace {
std::unique_lock<std::mutex> lockMapReaders(std::mutex& mutex,const std::atomic_bool& cancelled) {
    std::unique_lock lock(mutex,std::defer_lock);
    while(!lock.try_lock()) {
        if(cancelled.load()) throw std::runtime_error("Map reader wait cancelled");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(cancelled.load()) throw std::runtime_error("Map preparation cancelled");
    return lock;
}
}
MapLibrary::Progress MapLibrary::progress() const {
    if(!pending_) return {};
    const uint64_t working=decoder_.reservedBytes()+pending_->uploadBytes;
    return {true,decoder_.busy(),bool(pending_->upload),pending_->page,
        pending_->plan?pending_->plan->pageCount():0,working,pending_->result->rgbaBytes+working};
}
void MapLibrary::cancelPending() noexcept {
    decoder_.cancel(); // A page task borrows pending_->plan; never reset it first.
    pending_.reset(); // Upload destructor waits for its own submitted chunk.
}
std::shared_ptr<const MapResources> MapLibrary::acquireProgressive(const std::filesystem::path& path,
    component::AssetsReaderState& readers,graphics::IDevice* device,graphics::IRenderPass* pass) {
    struct Timing {
        double& maximum;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
        ~Timing() {maximum=std::max(maximum,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
    } timing{maxOwnerCallMs_};
    if(!device || !pass || (device_ && device_!=device) || (pass_ && pass_!=pass))
        throw std::invalid_argument("Map library requires one live device/render pass");
    const auto key=std::filesystem::absolute(path).lexically_normal();
    if(pending_ && pending_->key!=key) throw std::logic_error("Cancel the previous map before changing requests");
    if(auto found=maps_.find(key)) return found;
    std::shared_ptr<const MapResources> restored;
    if(retiredMaps_.restore(key,[&](const auto& map,uint64_t bytes){maps_.insert(key,map,bytes);restored=map;})) return restored;
    try {
        if(!pending_) {
            pending_=std::make_unique<PendingMap>();pending_->key=key;device_=device;pass_=pass;
            decoder_.submit([key,&readers](const auto& cancelled) {
                MapDecodeWorker::Result output;
                auto lock=lockMapReaders(*readers.queryMutex,cancelled);
                constexpr auto version=CrossGateVersion::CG_VERSION_Classic;
                if(!readers.pGfxInfoReader[version] || !readers.pGfxDataReader[version] || !readers.pPaletteReader)
                    throw std::runtime_error("Map requires initialized Classic readers");
                MapLayout layout;
                if(IMapLayoutReader::create()->read(key,&layout)!=Results::Success)
                    throw std::runtime_error("Could not read MAP layout: "+key.string());
                output.plan=std::make_unique<MapBuilder>(std::move(layout),*readers.pGfxInfoReader[version],
                    *readers.pPaletteReader,&cancelled);return output;
            },0);
            return nullptr;
        }
        auto& pending=*pending_;
        if(pending.upload) {
            const auto state=pending.upload->pollUpload();
            if(state==graphics::SpriteUploadState::Failed) throw std::runtime_error("Progressive map upload failed");
            if(state!=graphics::SpriteUploadState::Ready) return nullptr;
            pending.result->renderers.push_back(std::move(pending.upload));
            pending.result->rgbaBytes+=pending.uploadBytes;pending.uploadBytes=0;++pending.page;
            // Do not start another page/upload in the same call as an upload poll.
            return nullptr;
        }
        if(auto output=decoder_.tryTake()) {
            if(output->error) std::rethrow_exception(output->error);
            if(output->plan) pending.plan=std::move(output->plan);
            else {
                pending.uploadBytes=uint64_t(output->pixels.width)*output->pixels.height*4;
                pending.upload=graphics::ISpriteRenderer::createPending(device,pass,output->pixels.width,
                    output->pixels.height,1,std::move(output->pixels.rgba),4*1024*1024);
                if(!pending.upload) throw std::runtime_error("Could not start map atlas upload");
                return nullptr;
            }
        }
        if(decoder_.busy()) return nullptr;
        if(pending.page==pending.plan->pageCount()) {
            auto data=std::move(*pending.plan).takeData();
            auto result=pending.result;
            result->layout=std::move(data.layout);result->images=std::move(data.images);
            result->rgbaBytes=data.rgbaBytes;result->reachX=data.reachX;result->reachY=data.reachY;
            maps_.insert(key,result,result->rgbaBytes);pending_.reset();
            LOGI("Map ready: "<<result->layout.width<<"x"<<result->layout.height<<", images="<<result->images.size()
                <<", atlas pages="<<result->renderers.size()<<", texture bytes="<<result->rgbaBytes);
            return result;
        }
        const auto page=pending.page;auto* plan=pending.plan.get();
        decoder_.submit([plan,page,&readers](const auto& cancelled) {
            auto lock=lockMapReaders(*readers.queryMutex,cancelled);
            MapDecodeWorker::Result output;
            output.pixels=plan->buildPage(page,*readers.pGfxDataReader[CrossGateVersion::CG_VERSION_Classic],
                MapDecodeWorker::MaxBytes,&cancelled);return output;
        },plan->pageBytes(page));
        peakWorkingBytes_=std::max(peakWorkingBytes_,plan->pageBytes(page));
        peakPendingBytes_=std::max(peakPendingBytes_,pending.result->rgbaBytes+plan->pageBytes(page));
        return nullptr;
    } catch(...) {cancelPending();throw;}
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
