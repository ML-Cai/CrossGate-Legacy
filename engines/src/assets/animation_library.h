#pragma once

#include "assets/animation_builder.h"
#include "assets/animation_decode_worker.h"
#include "assets/idle_resource_cache.h"
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
    ~AnimationLibrary() { clear(); }
    std::shared_ptr<const AnimationClip> acquire(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    // Serialized render-thread path: returns nullptr until upload completion.
    // At most one pending clip (<=32 MiB decoded RGBA, <=4 MiB staging).
    // Different requests return nullptr while that slot is occupied; retry later.
    // Background CPU decode is enabled by default; poll once per frame.
    // Reader storage must outlive clear()/destruction. All GPU work stays here.
    std::shared_ptr<const AnimationClip> acquireProgressive(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass, bool backgroundDecode=true);
    std::shared_ptr<const AnimationClip> findReady(const AnimationRequest& request) const {
        return clips_.find(request);
    }
    uint64_t pendingBytes() const noexcept {
        if(!pending_) return decoder_.reservedBytes();
        const auto& c=pending_->second;return uint64_t(c->width)*c->height*c->frameCount*4;
    }
    struct Usage {size_t clips=0;uint64_t rgbaBytes=0,pendingBytes=0,budgetBytes=0,evictions=0,evictedBytes=0;};
    Usage usage() const noexcept {
        return {clips_.size(),clips_.bytes(),pendingBytes(),textureBudget_,clips_.evictions(),clips_.evictedBytes()};
    }
    // Cancellation/scene exit can wait for the one already-submitted chunk.
    void cancelPending() noexcept { decoder_.cancel(); decodeRequest_.reset(); decodeReaders_=nullptr; pending_.reset(); builder_.clear(); }
    void setTextureBudget(uint64_t bytes) {
        const uint64_t used=clips_.bytes()+pendingBytes();
        if (!bytes || used > bytes) throw std::invalid_argument("Texture budget below live cached payload");
        textureBudget_ = bytes;
    }
    // Blocking maintenance only: waits for this device if idle victims exist.
    // Held clips and in-flight decode/upload slots are never eviction candidates.
    uint64_t trimUnused(uint64_t targetBytes);
    // Release scene/entity references and wait for GPU work before clearing.
    void clear() noexcept {
        decoder_.stop(); decodeRequest_.reset(); decodeReaders_=nullptr;
        pending_.reset(); // Waits for only its own submitted chunk, if any.
        clips_.clear();
        builder_.clear();
        device_ = nullptr;
        pass_ = nullptr;
    }
private:
    uint64_t remainingBytes() const;
    std::shared_ptr<const AnimationClip> uploadClip(const AnimationRequest& request,AnimationPixels pixels,
        graphics::IDevice* device,graphics::IRenderPass* pass,bool progressive);
    AnimationDecodeWorker decoder_;
    std::optional<AnimationRequest> decodeRequest_;
    component::AssetsReaderState* decodeReaders_=nullptr;
    std::shared_ptr<const AnimationClip> buildClip(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass, bool progressive);
    std::optional<std::pair<AnimationRequest,std::shared_ptr<const AnimationClip>>> pending_;
    uint64_t textureBudget_ = 256ull*1024*1024;
    AnimationBuilder builder_;
    IdleResourceCache<AnimationRequest,AnimationClip> clips_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
