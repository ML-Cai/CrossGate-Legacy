// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include "cgl/graphics/buffer.h"

namespace cgl {
namespace vk {

class Device;

class Buffer : public cgl::IBuffer {
 public:
    Buffer(cgl::IBuffer::Types type,
           size_t              bufferCapacity,
           const char*         pName);

    ~Buffer();

    cgl::IBuffer::Types type() const noexcept override { return type_; }

    size_t capacity() const noexcept override { return bufferCapacity_; }

    void destroy();

    bool createInternal(cgl::vk::Device* pDevice);

    VkBuffer buffer() const { return buffer_; }

 private:
    const cgl::IBuffer::Types type_;
    const uint64_t  bufferCapacity_;
    std::string     name_;
    VkDevice        device_;
    VkBuffer        buffer_;
    VkDeviceMemory  bufferMem_;

};

}   // namespace vk
}   // namespace cgl
