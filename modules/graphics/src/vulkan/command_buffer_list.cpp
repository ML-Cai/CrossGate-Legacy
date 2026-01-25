// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/graphics/device.h"
#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::CommandBufferList;
using cgl::graphics::vulkan::CommandBuffer;

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::CommandBufferList
// -----------------------------------------------------------------------------
CommandBufferList::CommandBufferList(
    cgl::graphics::IDevice* pDevice,
    uint32_t                bufferCount)
    : cmdPool_(VK_NULL_HANDLE) {
    auto pNativeDevice   = reinterpret_cast<cgl::graphics::vulkan::Device *>(pDevice);
    device_              = pNativeDevice->device();
    graphicsQueueFamily_ = pNativeDevice->graphicsQueueFamily();
    cmdBuffers_.resize(bufferCount);
}

CommandBufferList::~CommandBufferList() {
    destroy();
}

void CommandBufferList::destroy() {
    if (cmdPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, cmdPool_, nullptr);
        cmdPool_ = VK_NULL_HANDLE;
    }
}

bool CommandBufferList::prepare() {
    LOGI("Prepare CommandBufferList with " << bufferCount() << " buffers.");

    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;

    return true;
}

bool CommandBufferList::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily_,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateCommandPool(device_, &poolInfo, nullptr, &cmdPool_),
        "Failed to create command pool");

    return true;
}

bool CommandBufferList::createCommandBuffers() {
    for (size_t i = 0 ; i < cmdBuffers_.size() ; i++) {
        auto cmdBuffer = std::make_unique<cgl::graphics::vulkan::CommandBuffer>(
                            device_, cmdPool_, static_cast<uint32_t>(i));

        if (!cmdBuffer->prepare()) {
            LOGE("Failed to create command buffer, index:" << i);
            return false;
        }
        cmdBuffers_[i] = std::move(cmdBuffer);
    }

    return true;
}


// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::CommandBuffer
// -----------------------------------------------------------------------------
CommandBuffer::CommandBuffer(VkDevice device, VkCommandPool pool, uint32_t idx)
    : device_(device),
      pool_(pool),
      cmdBuffer_(VK_NULL_HANDLE),
      idx_(idx) {
}

CommandBuffer::~CommandBuffer() {
}

bool CommandBuffer::reset() {
    auto result = vkResetCommandBuffer(cmdBuffer_, 0);
    if (result != VK_SUCCESS) {
        LOGE("Failed to reset the command buffer.");
        return false;
    }

    return true;
}

bool CommandBuffer::begin() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    auto result = vkBeginCommandBuffer(cmdBuffer_, &beginInfo);
    if (result != VK_SUCCESS) {
        LOGE("Failed to begin the command buffer.");
        return false;
    }

    return true;
}

bool CommandBuffer::end() {
    auto result = vkEndCommandBuffer(cmdBuffer_);
    if (result != VK_SUCCESS) {
        LOGE("Failed to end the command buffer.");
        return false;
    }

    return true;
}

void CommandBuffer::destroy() {
    cmdBuffer_ = VK_NULL_HANDLE;
}

bool CommandBuffer::prepare() {
    LOGD("Prepare CommandBuffer(Vulkan), index: " << idx());

    return createCommandBuffer();
}

bool CommandBuffer::createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool_,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer_),
        "Failed to create command buffers");

    return true;
}
