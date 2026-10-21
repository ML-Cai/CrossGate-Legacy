#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "cgl/common/viewport.h"

namespace cgl::graphics {
class IDevice;
class IRenderPass;
class ICommandBuffer;

enum class SpriteUploadState { Pending, Ready, Failed };

struct SpriteRegion {
    uint32_t x = 0, y = 0, width = 0, height = 0;
};

// Immutable, equally sized RGBA8 frames. Upload once, select a layer per draw.
// Device must outlive the renderer; retire recorded draws before destruction.
// Drawing requires a compatible live pass; shared pipeline state owns its lifetime.
class ISpriteRenderer {
public:
    using Ptr = std::unique_ptr<ISpriteRenderer>;
    virtual ~ISpriteRenderer() = default;
    // Rows are top-down, frames are contiguous. Uses straight (not premultiplied)
    // alpha and nearest filtering. Requires a single-sample color-only render pass
    // from the same Vulkan device. Upload waits for its own fence on the graphics queue;
    // callers must serialize creation with other work submitted to that queue.
    static Ptr create(IDevice* device, IRenderPass* pass,
                      uint32_t width, uint32_t height, uint32_t frameCount,
                      std::span<const uint8_t> rgba);
    // Moves ownership of CPU pixels. Starts at most one bounded copy submission.
    // Poll once per frame on the render thread; never draw until Ready. Destroying
    // a pending renderer waits for its own submitted fence before freeing resources.
    // A chunk must contain at least one complete RGBA row. No worker/queue concurrency.
    static Ptr createPending(IDevice* device, IRenderPass* pass,
                      uint32_t width, uint32_t height, uint32_t frameCount,
                      std::vector<uint8_t> rgba, size_t chunkBytes = 4*1024*1024);
    // Nonblocking fence query; may submit ONE next chunk. Ready/Failed are terminal.
    virtual SpriteUploadState pollUpload() = 0;
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
