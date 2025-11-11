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

using cgl::vk::Buffer;

// -----------------------------------------------------------------------------
Buffer::Buffer(
    cgl::IBuffer::Types type,
    size_t              bufferCapacity,
    const char*         pName)
    : type_(type),
      bufferCapacity_(bufferCapacity),
      name_(pName),
      device_(VK_NULL_HANDLE),
      buffer_(VK_NULL_HANDLE),
      bufferMem_(VK_NULL_HANDLE) {
}

Buffer::~Buffer() {
    destroy();
}

void Buffer::destroy() {
    CGL_SAFE_FREE_BUFFER(device_, buffer_);
    CGL_SAFE_FREE_MEMORY(device_, bufferMem_);
}

bool Buffer::createInternal(cgl::vk::Device* pDevice) {
    LOGD("Create buffer(" << name_ << ") with size:" << capacity());

    if ((pDevice == nullptr) || (capacity() == 0)) {
        LOGE("Invalid create args for cgl::vk::Buffer");
        return false;
    }

    device_ = pDevice->device();

    VkBufferUsageFlags bufferUsage;
    switch (type_) {
        case cgl::IBuffer::Types::VertexBuffer:
            bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case cgl::IBuffer::Types::IndexBuffer:
            bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case cgl::IBuffer::Types::UniformBuffer:
            bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        default:
            LOGE("Invalid buffer object type:" << static_cast<int>(type_));
            return false;
    }

    RETURN_FALSE_IF_ERROR(
        pDevice->createBuffer(
            this->capacity(),
            (bufferUsage |
             VK_BUFFER_USAGE_TRANSFER_DST_BIT |
             VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &buffer_,
            &bufferMem_),
        "Failed to create staging buffer from device.");

    return true;
}
