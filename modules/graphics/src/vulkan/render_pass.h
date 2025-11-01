// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <vulkan/vulkan_core.h>
#include "cgl/graphics/render_pass.h"


namespace cgl {
namespace vk {

class IRenderPass : public cgl::IRenderPass {
 public:
    explicit IRenderPass(VkDevice device)
        : device_(device), renderPass_(VK_NULL_HANDLE) {}

    virtual ~IRenderPass() = default;

    VkRenderPass renderPass() const { return renderPass_; }

    VkDevice device() const { return device_; }

 protected:
    // RenderPass related data
    VkRenderPass renderPass_;

 private:
    VkDevice device_;
};

}   // namespace vk
}   // namespace cgl
