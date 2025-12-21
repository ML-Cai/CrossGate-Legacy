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
#include <vector>
#include "cgl/graphics/framebuffer.h"
#include "vulkan/render_pass.h"

namespace cgl {
namespace graphics {
namespace vulkan {

class CommandBuffer;

class InitSceneRenderPass : public cgl::graphics::vulkan::IRenderPass {
 public:
    static constexpr uint32_t COLOR_ATTACTMENT_IDX = 0;

    explicit InitSceneRenderPass(VkDevice device);

    virtual ~InitSceneRenderPass();

    void destroy();

    bool prepare(cgl::graphics::ISwapchain* pSwapchain);

    bool begin(const cgl::Viewport&           viewport,
               cgl::graphics::ICommandBuffer* pCmdBuffer,
               cgl::graphics::IFramebuffer*   pFramebuffer) override;

    bool end() override;

    cgl::graphics::IFramebuffer* acquireFramebuffer(uint32_t idx) const override {
        if (idx >= frameBufferList_.size()) return nullptr;
        return frameBufferList_[idx].get();
    }

 protected:
    bool createFramebuffer(cgl::graphics::ISwapchain* pSwapchain);

 private:
    bool createRenderPass(cgl::graphics::ISwapchain* pSwapchain);

    std::vector<cgl::graphics::IFramebuffer::Ptr> frameBufferList_;
    cgl::graphics::ICommandBuffer* pLastICmdBuffer_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
