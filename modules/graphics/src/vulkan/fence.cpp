// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "vulkan/fence.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::Fence;

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::Fence
// -----------------------------------------------------------------------------
Fence::Fence(VkDevice device)
    : fence_(VK_NULL_HANDLE) {
    device_ = device;
}

Fence::~Fence() {
    destroy();
}

void Fence::destroy() {
    if (fence_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, fence_, nullptr);
        fence_ = nullptr;
    }
}

bool Fence::prepare() {
    LOGD("Prepare Fence(Vulkan) ...");

    VkFenceCreateInfo fenceInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateFence(device_, &fenceInfo, nullptr, &fence_),
        "Failed to create fence");

    return true;
}

bool Fence::wait() {
    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX),
        "Failed to wait the fence");

    return true;
}

bool Fence::reset() {
    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkResetFences(device_, 1, &fence_),
        "Failed to reset the fence");

    return true;
}
