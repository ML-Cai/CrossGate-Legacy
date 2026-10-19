#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include "cgl/common/viewport.h"

namespace cgl::graphics {
class IDevice;
class IRenderPass;
class ICommandBuffer;

struct SpriteRegion {
    uint32_t x = 0, y = 0, width = 0, height = 0;
};

// Immutable, equally sized RGBA8 frames. Upload once, select a layer per draw.
// Device and render pass must outlive the renderer; destroy after GPU completion.
class ISpriteRenderer {
public:
    using Ptr = std::unique_ptr<ISpriteRenderer>;
    virtual ~ISpriteRenderer() = default;
    // Rows are top-down, frames are contiguous. Uses straight (not premultiplied)
    // alpha and nearest filtering. Requires a single-sample color-only render pass
    // from the same Vulkan device. Upload is synchronous on the graphics queue;
    // callers must serialize creation with other work submitted to that queue.
    static Ptr create(IDevice* device, IRenderPass* pass,
                      uint32_t width, uint32_t height, uint32_t frameCount,
                      std::span<const uint8_t> rgba);
    // Record inside the compatible render pass. Coordinates are relative to the
    // viewport's top-left, in pixels. Invalid frames/scales are ignored. Sets the
    // graphics pipeline, descriptors, viewport and scissor; later draws must bind
    // their own state. The viewport must fit within the active framebuffer.
    virtual void draw(ICommandBuffer* command, const cgl::Viewport& viewport,
                      uint32_t frame, float x, float y, float scale) = 0;
    // Draw a pixel rectangle from one frame; output size is region size * scale.
    // Invalid or empty source rectangles are ignored, with the same state rules.
    virtual void drawRegion(ICommandBuffer* command, const cgl::Viewport& viewport,
                            uint32_t frame, SpriteRegion source, float x, float y, float scale) = 0;
};
}
