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
    // Serialized render-thread path: returns nullptr until upload completion.
    // At most one pending clip (<=32 MiB decoded RGBA, <=4 MiB staging).
    // Different requests return nullptr while that slot is occupied; retry later.
    // CPU decode/allocation is still synchronous. Poll this once per frame.
    std::shared_ptr<const AnimationClip> acquireProgressive(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    uint64_t pendingBytes() const noexcept {
        if(!pending_) return 0;
        const auto& c=pending_->second;return uint64_t(c->width)*c->height*c->frameCount*4;
    }
    // Cancellation/scene exit can wait for the one already-submitted chunk.
    void cancelPending() noexcept { pending_.reset(); }
    void setTextureBudget(uint64_t bytes) {
        uint64_t used = 0;
        for (const auto& [key, clip] : clips_) used += uint64_t(clip->width)*clip->height*clip->frameCount*4;
        if(pending_) {const auto& c=pending_->second;used+=uint64_t(c->width)*c->height*c->frameCount*4;}
        if (!bytes || used > bytes) throw std::invalid_argument("Texture budget below live cached payload");
        textureBudget_ = bytes;
    }
    // Release scene/entity references and wait for GPU work before clearing.
    void clear() noexcept {
        pending_.reset(); // Waits for only its own submitted chunk, if any.
        clips_.clear();
        builder_.clear();
        device_ = nullptr;
        pass_ = nullptr;
    }
private:
    std::shared_ptr<const AnimationClip> buildClip(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass, bool progressive);
    std::optional<std::pair<AnimationRequest,std::shared_ptr<const AnimationClip>>> pending_;
    uint64_t textureBudget_ = 256ull*1024*1024;
    AnimationBuilder builder_;
    std::map<AnimationRequest, std::shared_ptr<const AnimationClip>> clips_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
