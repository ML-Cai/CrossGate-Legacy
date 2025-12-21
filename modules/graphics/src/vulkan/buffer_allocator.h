// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <memory>
#include <vulkan/vulkan_core.h>
#include "cgl/trace/logger.h"
#include "cgl/graphics/buffer_allocator.h"
#include "cgl/graphics/buffer.h"

namespace cgl {

namespace vk {
class Device;
}

namespace graphics {
namespace vulkan {

// -----------------------------------------------------------------------------
class BufferAllocator : public cgl::graphics::IBufferAllocator {
 public:
    static cgl::graphics::IBufferAllocator::Ptr create(
        cgl::graphics::IBuffer::Types  bufferType,
        cgl::graphics::vulkan::Device* pVkDevice,
        size_t                         poolSize);

    explicit BufferAllocator(
        cgl::graphics::IBuffer::Types bufferType,
        VkDevice                      vkDevice,
        size_t                        poolSize
    );

    ~BufferAllocator();

    cgl::graphics::IBuffer::Ptr alloc(
        size_t      size,
        const char* pName = "") override;

    cgl::graphics::IBuffer::Types bufferType() const noexcept override {
        return bufferType_;
    }

    size_t poolSize() const noexcept override {
        return poolSize_;
    }

 private:
    bool allocMemory(
        VkDevice                         vkDevice,
        VkPhysicalDeviceMemoryProperties memPropertiese,
        VkMemoryPropertyFlags            property,
        size_t                           size);

    VkDevice vkDevice_;
    VkDeviceMemory vkMemory_;
    VkDeviceSize memoryOffset_;

    cgl::graphics::IBuffer::Types bufferType_;
    size_t poolSize_;
};

// -----------------------------------------------------------------------------
}   // namespace vk
}   // namespace graphics
}   // namespace cgl
