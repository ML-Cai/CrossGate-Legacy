// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "vulkan/semaphore.h"
#include "vulkan/common.h"

using cgl::vk::Semaphore;

// -----------------------------------------------------------------------------
// cgl::vk::Semaphore
// -----------------------------------------------------------------------------
Semaphore::Semaphore(VkDevice device)
    : device_(device),
      semaphore_(VK_NULL_HANDLE) {
}

Semaphore::~Semaphore() {
    destroy();
}

void Semaphore::destroy() {
    if (semaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, semaphore_, nullptr);
        semaphore_ = nullptr;
    }
}

bool Semaphore::prepare() {
    LOGD("Prepare Semaphore(Vulkan) ...");

    VkSemaphoreCreateInfo semaphoreInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore_),
        "Failed to create semaphore");

    return true;
}
