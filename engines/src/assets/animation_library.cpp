#include "assets/animation_library.h"
#include "assets/assets_components.h"
#include "cgl/trace/logger.h"
#include <stdexcept>
#include <algorithm>

namespace cgl {
std::shared_ptr<const AnimationClip> AnimationLibrary::acquire(
    const AnimationRequest& request, component::AssetsReaderState& readers,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if (const auto found = clips_.find(request); found != clips_.end()) return found->second;
    if(pending_) throw std::runtime_error("Synchronous acquire cannot create another clip while an upload is pending");
    auto clip=buildClip(request,readers,device,pass,false);
    clips_.emplace(request,clip);
    return clip;
}
std::shared_ptr<const AnimationClip> AnimationLibrary::acquireProgressive(
    const AnimationRequest& request,component::AssetsReaderState& readers,
    graphics::IDevice* device,graphics::IRenderPass* pass) {
    if(!device || !pass || (device_ && device_!=device) || (pass_ && pass_!=pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if(const auto found=clips_.find(request);found!=clips_.end()) return found->second;
    if(pending_) {
        const auto status=pending_->second->renderer->pollUpload();
        if(status==graphics::SpriteUploadState::Failed) {
            pending_.reset();throw std::runtime_error("Progressive animation upload failed");
        }
        if(status==graphics::SpriteUploadState::Ready) {
            clips_.emplace(pending_->first,pending_->second);pending_.reset();
            if(const auto found=clips_.find(request);found!=clips_.end()) return found->second;
        }
        return nullptr; // Do not start another submission in this same call.
    }
    auto clip=buildClip(request,readers,device,pass,true);
    pending_=std::pair{request,std::move(clip)};
    return nullptr;
}
std::shared_ptr<const AnimationClip> AnimationLibrary::buildClip(
    const AnimationRequest& request,component::AssetsReaderState& readers,
    graphics::IDevice* device,graphics::IRenderPass* pass,bool progressive) {
    const uint64_t maxPreviewBytes = textureBudget_;
    uint64_t cachedBytes = 0;
    for (const auto& [key, cached] : clips_)
        cachedBytes += uint64_t(cached->width) * cached->height * cached->frameCount * 4;
    if (cachedBytes > maxPreviewBytes) throw std::runtime_error("Animation cache exceeds texture budget");
    auto pixels = builder_.build(request, readers, progressive ? std::min<uint64_t>(maxPreviewBytes-cachedBytes,32ull*1024*1024) : maxPreviewBytes-cachedBytes);
    auto renderer = progressive ? graphics::ISpriteRenderer::createPending(device,pass,
        pixels.width,pixels.height,pixels.frameCount,std::move(pixels.rgba)) :
        graphics::ISpriteRenderer::create(device,pass,pixels.width,pixels.height,pixels.frameCount,pixels.rgba);
    if (!renderer) throw std::runtime_error("Could not upload animation or create sprite pipeline");
    auto result = std::make_shared<AnimationClip>();
    result->renderer = std::move(renderer);
    result->offsetX = pixels.offsetX;
    result->offsetY = pixels.offsetY;
    result->width = pixels.width;
    result->height = pixels.height;
    result->frameCount = pixels.frameCount;
    result->durationMs = pixels.durationMs;
    device_ = device;
    pass_ = pass;
    LOGI((progressive ? "Character animation pending: serial=" : "Character animation ready: serial=") << request.serial << ", frames=" << pixels.frameCount
         << ", duration=" << pixels.durationMs << "ms, canvas=" << pixels.width << "x" << pixels.height);
    return result;
}
}
