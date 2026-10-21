#pragma once

#include "assets/animation_builder.h"
#include <map>
#include <memory>
#include <stdexcept>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
// Immutable shared clip. Playback state belongs to individual entities.
struct AnimationClip {
    graphics::ISpriteRenderer::Ptr renderer;
    uint32_t width = 0, height = 0, frameCount = 0, durationMs = 0;
    int32_t offsetX = 0, offsetY = 0;
};
// Owned by ClientEngine. Current cache uses version/serial/direction/motion, daytime resources and one
// device/render pass. Clear after GPU completion, before destroying the device.
class AnimationLibrary {
public:
    std::shared_ptr<const AnimationClip> acquire(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    void setTextureBudget(uint64_t bytes) {
        uint64_t used = 0;
        for (const auto& [key, clip] : clips_) used += uint64_t(clip->width)*clip->height*clip->frameCount*4;
        if (!bytes || used > bytes) throw std::invalid_argument("Texture budget below live cached payload");
        textureBudget_ = bytes;
    }
    // Release scene/entity references and wait for GPU work before clearing.
    void clear() noexcept {
        clips_.clear();
        builder_.clear();
        device_ = nullptr;
        pass_ = nullptr;
    }
private:
    uint64_t textureBudget_ = 256ull*1024*1024;
    AnimationBuilder builder_;
    std::map<AnimationRequest, std::shared_ptr<const AnimationClip>> clips_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
