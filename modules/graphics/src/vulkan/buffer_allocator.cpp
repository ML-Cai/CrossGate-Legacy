// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <assert.h>
#include <atomic>
#include <memory>
#include <vector>
#include <queue>
#include <sstream>
#include <mutex>
#include "cgl/trace/logger.h"
#include "cgl/graphics/buffer.h"
#include <vulkan/vk_enum_string_helper.h>
#include "vulkan/device.h"
#include "vulkan/buffer_allocator.h"
#include "vulkan/buffer.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::BufferAllocator;

// -----------------------------------------------------------------------------
// anonymous namespace
// -----------------------------------------------------------------------------
namespace {

VkBufferUsageFlags lookupUsage(cgl::graphics::IBuffer::Types type) {
    switch (type) {
    case cgl::graphics::IBuffer::Types::VertexBuffer:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case cgl::graphics::IBuffer::Types::IndexBuffer:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case cgl::graphics::IBuffer::Types::UniformBuffer:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    default:
        return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    }
}

VkDeviceSize alignUp(VkDeviceSize offset, VkDeviceSize alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

uint32_t findSuitableMemoryTypeIndex(
    VkPhysicalDeviceMemoryProperties memProperties,
    uint32_t                         memTypeBits,
    VkMemoryPropertyFlags            properties)
{
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return VK_MAX_MEMORY_TYPES;
}

uint32_t findMemoryType(
    VkDevice                         vkDevice,
    VkPhysicalDeviceMemoryProperties memPropertiese,
    VkBufferUsageFlags               usage,
    VkMemoryPropertyFlags            properties,
    VkDeviceSize                     poolSize
) {
    // create a temp buffer to query memory requiretments
    VkBuffer tempBuffer = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = poolSize;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &tempBuffer) != VK_SUCCESS) {
        LOGE("Failed to create temp buffer(size: " << poolSize
             << ") for alloactor");
        return VK_MAX_MEMORY_TYPES;
    }

    // get memory req
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(vkDevice, tempBuffer, &memReqs);
    vkDestroyBuffer(vkDevice, tempBuffer, nullptr);

    // find memory idx
    return findSuitableMemoryTypeIndex(memPropertiese,
                                       memReqs.memoryTypeBits,
                                       properties);
}

}   // namespace


// -----------------------------------------------------------------------------
// anonymous namespace
// -----------------------------------------------------------------------------
BufferAllocator::BufferAllocator(
    cgl::graphics::IBuffer::Types bufferType,
    VkDevice                      vkDevice,
    size_t                        poolSize)
    : vkDevice_(vkDevice),
      vkMemory_(VK_NULL_HANDLE),
      memoryOffset_(0),
      bufferType_(bufferType),
      poolSize_(poolSize) {
}

// -----------------------------------------------------------------------------
BufferAllocator::~BufferAllocator() {
    CGL_SAFE_FREE_MEMORY(vkDevice_, vkMemory_);
}

// -----------------------------------------------------------------------------
bool BufferAllocator::allocMemory(
    VkDevice                         vkDevice,
    VkPhysicalDeviceMemoryProperties memPropertiese,
    VkMemoryPropertyFlags            property,
    size_t                           size
) {
    // prepare alloc info
    VkMemoryAllocateInfo allocInfo {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = size,
        .memoryTypeIndex = findMemoryType(vkDevice,
                                          memPropertiese,
                                          lookupUsage(this->bufferType()),
                                          property,
                                          size)
    };
    LOGD("Allocator allocate " << size << " bytes as the pool"
         << ", memoryTypeIndex: " << allocInfo.memoryTypeIndex
         << ", MemoryPropertyFlags: " << string_VkMemoryPropertyFlags(property));

    if (allocInfo.memoryTypeIndex == VK_MAX_MEMORY_TYPES) {
        LOGE("No valid memory type index found to alloc memory");
        return false;
    }

    // Alocate memory for the buffer
    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateMemory(vkDevice_, &allocInfo, nullptr, &vkMemory_),
        "Failed to allocate buffer memory!");

    return true;
}

// -----------------------------------------------------------------------------
cgl::graphics::IBuffer::Ptr BufferAllocator::alloc(
    size_t      bufferSize,
    const char* pBufferName
) {
    LOGT("Allocate " << bufferSize << " bytes from allocator.");

    // Create buffer
    VkBufferCreateInfo bufferInfo {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = bufferSize,
        .usage       = lookupUsage(this->bufferType()) |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |   // default enable Tx
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,    // default enable Rx
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    assert(bufferInfo.usage != VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM);

    VkBuffer vkBuffer;
    if (vkCreateBuffer(vkDevice_, &bufferInfo, nullptr, &vkBuffer) != VK_SUCCESS) {
        LOGE("Failed to create vkBuffer.");
        return nullptr;
    }

    // Get buffer memory requirements
    VkMemoryRequirements vkMemReqs;
    vkGetBufferMemoryRequirements(vkDevice_, vkBuffer, &vkMemReqs);

    // check alloc info
    VkDeviceSize nextMemOffset = alignUp(memoryOffset_ + vkMemReqs.size,
                                         vkMemReqs.alignment);
    if (nextMemOffset > poolSize_) {
        CGL_SAFE_FREE_BUFFER(vkDevice_, vkBuffer);
        LOGE("No enough chunk in the pool to allocate the buffer."
             << ", try to alloc " << vkMemReqs.size
             << " bytes from " << poolSize_ << " byte in the pool." );
        return nullptr;
    }

    // bind to memory
    if (vkBindBufferMemory(vkDevice_, vkBuffer, vkMemory_, memoryOffset_) != VK_SUCCESS) {
        LOGE("Failed to bind buffer memory");
        return nullptr;
    }
    memoryOffset_ = nextMemOffset;

    return std::make_unique<cgl::graphics::vulkan::Buffer>(
                vkDevice_, vkBuffer, vkMemory_, vkMemReqs,
                this->bufferType(), bufferSize, 0,
                pBufferName);
}

// -----------------------------------------------------------------------------
// static members of cgl::graphics::vulkan::BufferAllocator
// -----------------------------------------------------------------------------
cgl::graphics::IBufferAllocator::Ptr BufferAllocator::create(
    cgl::graphics::IBuffer::Types  bufferType,
    cgl::graphics::vulkan::Device* pVkDevice,
    size_t                         poolSize
) {
    if ((pVkDevice == nullptr) ||
        (bufferType == cgl::graphics::IBuffer::Types::Unknown) ||
        (poolSize < 64)) {
        LOGE("Invalid args to create BufferAllocator"
             << ", device " << static_cast<void *>(pVkDevice)
             << ", type: " << cgl::graphics::ToStr(bufferType)
             << ", poolSize: " << poolSize );
        return nullptr;
    }

    // create allocator object
    auto pAllocator = std::make_unique<BufferAllocator>(
                        bufferType,
                        pVkDevice->handle(),
                        poolSize);
    assert(pAllocator != nullptr);

    // allocate device local memory
    VkMemoryPropertyFlags property = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (pAllocator->allocMemory(pVkDevice->handle(),
                                pVkDevice->physicalDeviceMemoryProperties(),
                                property,
                                poolSize) == false) {
        LOGE("Failed to allocate pool memory with size: "<< poolSize);
        return nullptr;
    }

    return pAllocator;
}
