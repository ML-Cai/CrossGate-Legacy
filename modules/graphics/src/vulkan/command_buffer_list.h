// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include <vector>
#include "cgl/graphics/command_buffer_list.h"

namespace cgl {

class IDevice;
namespace graphics {
namespace vulkan {

// -----------------------------------------------------------------------------
class CommandBuffer : public cgl::graphics::ICommandBuffer {
 public:
    explicit CommandBuffer(VkDevice device, VkCommandPool pool, uint32_t idx);

    virtual ~CommandBuffer();

    void destroy();

    bool reset() override;

    bool begin() override;

    bool end() override;

    uint32_t idx() const noexcept { return idx_; }

    bool prepare();

    VkCommandBuffer commandBuffer() const { return cmdBuffer_; }

 private:
    VkDevice device_;
    VkCommandPool pool_;
    VkCommandBuffer cmdBuffer_;
    uint32_t idx_;

    bool createCommandBuffer();
};

// -----------------------------------------------------------------------------
class CommandBufferList : public cgl::graphics::ICommandBufferList {
 public:
    explicit CommandBufferList(cgl::graphics::IDevice* pDevice,
                               uint32_t bufferCount);

    virtual ~CommandBufferList();

    void destroy();

    bool prepare();

    VkCommandPool commandPool() const { return cmdPool_; }

    cgl::graphics::ICommandBuffer* commandBuffer(uint32_t idx) override {
        return cmdBuffers_[idx].get();
    }

    uint32_t bufferCount() const noexcept override {
        return static_cast<uint32_t>(cmdBuffers_.size());
    }

 protected:
    bool createCommandBuffers() override;

 private:
    VkDevice device_;
    VkCommandPool cmdPool_;
    uint32_t graphicsQueueFamily_;
    std::vector<cgl::graphics::ICommandBuffer::Ptr> cmdBuffers_;

    bool createCommandPool();
};


}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
