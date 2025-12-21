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
}
}



namespace cgl {
namespace graphics {
namespace vulkan {

class Buffer : public cgl::graphics::IBuffer {
 public:
    explicit Buffer(
        VkDevice vkDevice,
        VkBuffer vkbuffer,
        VkDeviceMemory vkBufferMem,
        VkMemoryRequirements vkMemReqs,
        cgl::graphics::IBuffer::Types bufferType,
        size_t bufferCapacity,
        size_t bufferOffset,
        const std::string& name);

    ~Buffer();

    cgl::graphics::IBuffer::Types type() const noexcept override { return type_; }

    size_t capacity() const noexcept override { return bufferCapacity_; }

    size_t offset() const noexcept override { return bufferOffset_; }

    std::string_view name() const noexcept override { return name_; }

    VkBuffer buffer() const { return vkBuffer_; }

 private:
    VkDevice vkDevice_;
    VkBuffer vkBuffer_;
    VkDeviceMemory vkBufferMem_;
    VkMemoryRequirements vkMemReqs_;
    bool isRefMem_;
    cgl::graphics::IBuffer::Types type_;
    size_t bufferCapacity_;
    size_t bufferOffset_;
    std::string name_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
