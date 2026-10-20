#include "assets/animation_library.h"
#include "assets/assets_components.h"
#include "cgl/trace/logger.h"
#include <stdexcept>

namespace cgl {
std::shared_ptr<const AnimationClip> AnimationLibrary::acquire(
    const AnimationRequest& request, component::AssetsReaderState& readers,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if (const auto found = clips_.find(request); found != clips_.end()) return found->second;
    constexpr uint64_t maxPreviewBytes = 256ull * 1024 * 1024;
    uint64_t cachedBytes = 0;
    for (const auto& [key, cached] : clips_)
        cachedBytes += uint64_t(cached->width) * cached->height * cached->frameCount * 4;
    if (cachedBytes > maxPreviewBytes) throw std::runtime_error("Animation cache exceeds texture budget");
    auto pixels = builder_.build(request, readers, maxPreviewBytes - cachedBytes);
    auto renderer = graphics::ISpriteRenderer::create(device, pass,
        pixels.width, pixels.height, pixels.frameCount, pixels.rgba);
    if (!renderer) throw std::runtime_error("Could not upload animation or create sprite pipeline");
    auto result = std::make_shared<AnimationClip>();
    result->renderer = std::move(renderer);
    result->offsetX = pixels.offsetX;
    result->offsetY = pixels.offsetY;
    result->width = pixels.width;
    result->height = pixels.height;
    result->frameCount = pixels.frameCount;
    result->durationMs = pixels.durationMs;
    clips_.emplace(request, result);
    device_ = device;
    pass_ = pass;
    LOGI("Character animation ready: serial=" << request.serial << ", frames=" << pixels.frameCount
         << ", duration=" << pixels.durationMs << "ms, canvas=" << pixels.width << "x" << pixels.height);
    return result;
}
}
