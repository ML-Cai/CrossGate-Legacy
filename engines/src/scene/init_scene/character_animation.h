#pragma once

#include <chrono>
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
struct Settings;
namespace component { struct AssetsReaderState; }

// One scene-owned Classic/daytime preview. No shared cache or entity management.
class CharacterAnimation {
public:
    void load(const Settings& settings, component::AssetsReaderState& readers,
              graphics::IDevice* device, graphics::IRenderPass* pass);
    bool ready() const noexcept { return bool(renderer_); }
    void draw(graphics::ICommandBuffer* command, const Viewport& viewport) const;
    // Call after GPU completion and before destroying the scene render pass.
    void reset() noexcept { renderer_.reset(); }
private:
    graphics::ISpriteRenderer::Ptr renderer_;
    uint32_t width_ = 0, height_ = 0, frameCount_ = 0, durationMs_ = 0;
    float scale_ = 1;
    std::chrono::steady_clock::time_point started_;
};
}
