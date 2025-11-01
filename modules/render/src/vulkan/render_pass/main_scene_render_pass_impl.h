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
#include "cgl/core/destroyable.h"
#include "cgl/core/types.h"
#include "cgl/renderer/framebuffer.h"
#include "renderer/vulkan/render_pass.h"

namespace cgl {
namespace vk {

class CommandBuffer;

class MainSceneRenderPass : public cgl::vk::IRenderPass,
                            public cgl::IDestroyable {
 public:
    static constexpr uint32_t COLOR_ATTACTMENT_IDX = 0;
    static constexpr uint32_t DEPTH_ATTACTMENT_IDX = 1;

    explicit MainSceneRenderPass(
        const cgl::IRenderPass::CreateInfo& createInfo);

    virtual ~MainSceneRenderPass();

    void destroy() override;

    bool prepare();

    cgl::IFramebuffer* acquireFramebuffer(uint32_t idx) const override {
        if (idx >= frameBufferList_.size()) return nullptr;
        return frameBufferList_[idx].get();
    }

 protected:
    bool createFramebuffer() override;

    bool internalBegin() override;

    bool internalEnd() override;

 private:
    bool createRenderPass();

    // Depth resource related data
    VkImage depthImage_;
    VkDeviceMemory depathImageMemory_;
    VkImageView depthImageView_;
    bool createDepthResources();

    std::vector<cgl::IFramebuffer::Ptr> frameBufferList_;
};

}   // namespace vk
}   // namespace cgl
