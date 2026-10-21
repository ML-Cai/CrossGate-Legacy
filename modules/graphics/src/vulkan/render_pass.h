// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <vulkan/vulkan_core.h>
#include <memory>
#include "cgl/graphics/render_pass.h"


namespace cgl {
namespace graphics {
namespace vulkan {

struct SpritePipeline;

class IRenderPass : public cgl::graphics::IRenderPass {
 public:
    explicit IRenderPass(VkDevice device)
        : device_(device), renderPass_(VK_NULL_HANDLE) {}

    virtual ~IRenderPass() = default;

    VkRenderPass renderPass() const { return renderPass_; }

    VkDevice device() const { return device_; }

    // Lazily creates one immutable sprite pipeline for this pass; no global cache.
    // Calls must be serialized on the render thread. A new pass gets a new bundle.
    std::shared_ptr<SpritePipeline> acquireSpritePipeline();

 protected:
    // Call when retiring/recreating the native pass after submitted work completes.
    void releaseSpritePipeline() noexcept { spritePipeline_.reset(); }

    // RenderPass related data
    VkRenderPass renderPass_;

 private:
    std::shared_ptr<SpritePipeline> spritePipeline_;
    VkDevice device_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
