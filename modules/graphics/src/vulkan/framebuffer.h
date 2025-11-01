// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <vulkan/vulkan_core.h>
#include "cgl/graphics/framebuffer.h"

namespace cgl {
namespace vk {

class Framebuffer : public cgl::IFramebuffer {
 public:
    explicit Framebuffer(VkDevice device)
        : device_(device), buffer_(VK_NULL_HANDLE) {}

    virtual ~Framebuffer() {
        destroy();
    }

    void destroy() {
        if (device_ != VK_NULL_HANDLE && buffer_ != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, buffer_, nullptr);
            buffer_ = VK_NULL_HANDLE;
        }
    }

    VkFramebuffer buffer() const { return buffer_; }

    void setBuffer(VkFramebuffer buffer) { buffer_ = buffer; }

 private:
    VkDevice device_;
    VkFramebuffer buffer_;
};

}   // namespace vk
}   // namespace cgl
