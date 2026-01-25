// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================


#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/buffer.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::Buffer;

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::Buffe namespace
// -----------------------------------------------------------------------------
Buffer::Buffer(
    VkDevice vkDevice,
    VkBuffer vkBuffer,
    VkDeviceMemory vkBufferMem,
    VkMemoryRequirements vkMemReqs,
    cgl::graphics::IBuffer::Types bufferType,
    size_t bufferCapacity,
    size_t bufferOffset,
    const std::string& name)
    : vkDevice_(vkDevice),
      vkBuffer_(vkBuffer),
      vkBufferMem_(vkBufferMem),
      vkMemReqs_(vkMemReqs),
      type_(bufferType),
      bufferCapacity_(bufferCapacity),
      bufferOffset_(bufferOffset),
      name_(name) {
    isRefMem_ = (vkBufferMem != VK_NULL_HANDLE);
}

// -----------------------------------------------------------------------------
Buffer::~Buffer() {
    CGL_SAFE_FREE_BUFFER(vkDevice_, vkBuffer_);

    if (isRefMem_ == false) {
        CGL_SAFE_FREE_MEMORY(vkDevice_, vkBufferMem_);
    }
}
