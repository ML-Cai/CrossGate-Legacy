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
#include "cgl/render/framebuffer.h"
#include "vulkan/render_pass.h"

namespace cgl {
namespace vk {

class CommandBuffer;

class InitSceneRenderPass : public cgl::vk::IRenderPass {
 public:
    static constexpr uint32_t COLOR_ATTACTMENT_IDX = 0;

    explicit InitSceneRenderPass(VkDevice device);

    virtual ~InitSceneRenderPass();

    void destroy();

    bool prepare(cgl::ISwapchain* pSwapchain);

    bool begin(const cgl::Viewport& viewport,
               cgl::ICommandBuffer* pCmdBuffer,
               cgl::IFramebuffer*   pFramebuffer) override;

    bool end() override;

    cgl::IFramebuffer* acquireFramebuffer(uint32_t idx) const override {
        if (idx >= frameBufferList_.size()) return nullptr;
        return frameBufferList_[idx].get();
    }

 protected:
    bool createFramebuffer(cgl::ISwapchain* pSwapchain);

 private:
    bool createRenderPass(cgl::ISwapchain* pSwapchain);

    std::vector<cgl::IFramebuffer::Ptr> frameBufferList_;
    cgl::ICommandBuffer* pLastICmdBuffer_;
};

}   // namespace vk
}   // namespace cgl
