#pragma once

#include <compare>
#include <map>
#include <memory>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
struct AnimationRequest {
    uint32_t serial = 101406, direction = 0, motion = 5;
    auto operator<=>(const AnimationRequest&) const = default;
};
// Immutable shared clip. Playback state belongs to individual entities.
struct AnimationClip {
    graphics::ISpriteRenderer::Ptr renderer;
    uint32_t width = 0, height = 0, frameCount = 0, durationMs = 0;
    int32_t offsetX = 0, offsetY = 0;
};
// Owned by ClientEngine. Current cache uses Classic/daytime resources and one
// device/render pass. Clear after GPU completion, before destroying the device.
class AnimationLibrary {
public:
    std::shared_ptr<const AnimationClip> acquire(const AnimationRequest& request,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    // Release scene/entity references and wait for GPU work before clearing.
    void clear() { clips_.clear(); }
private:
    std::map<AnimationRequest, std::shared_ptr<const AnimationClip>> clips_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
