// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================


#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/staging_buffer.h"
#include "vulkan/common.h"

using cgl::graphics::vulkan::StagingBuffer;


// -----------------------------------------------------------------------------
// anonymous namespace
// -----------------------------------------------------------------------------
namespace {

VkResult findMemoryTypeIndex(
    uint32_t                         memoryTypeBits,
    VkPhysicalDeviceMemoryProperties physicalDeviceMemProperties,
    VkMemoryPropertyFlags            properties,
    uint32_t*                        pIndex
) {
    const auto& prop = physicalDeviceMemProperties;
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++) {
        if ((memoryTypeBits & (1 << i)) &&
            (prop.memoryTypes[i].propertyFlags & properties) == properties) {
            *pIndex = i;
            return VK_SUCCESS;
        }
    }

    return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

}   // namespace

// -----------------------------------------------------------------------------
// cgl::graphics::vulkan::StagingBuffer namespace
// -----------------------------------------------------------------------------
StagingBuffer::StagingBuffer(size_t bufferCapacity, const char* pName)
    : bufferCapacity_(bufferCapacity),
      name_(pName),
      device_(VK_NULL_HANDLE),
      buffer_(VK_NULL_HANDLE),
      bufferMem_(VK_NULL_HANDLE),
      pMapedData_(nullptr) {
}

StagingBuffer::~StagingBuffer() {
    destroy();
}

void StagingBuffer::destroy() {
    if (pMapedData_ != nullptr) vkUnmapMemory(device_, bufferMem_);

    CGL_SAFE_FREE_BUFFER(device_, buffer_);
    CGL_SAFE_FREE_MEMORY(device_, bufferMem_);
}

bool StagingBuffer::createNativeBuffer(
    cgl::graphics::vulkan::Device*            pVkDevice,
    const VkBufferUsageFlags    bufferUsages,
    const VkMemoryPropertyFlags properties
) {
    if ((pVkDevice == nullptr) || (capacity() == 0)) {
        LOGE("Invalid create args for cgl::graphics::vulkan::StagingBuffer");
        return false;
    }

    device_ = pVkDevice->handle();

    // Create buffer
    VkBufferCreateInfo bufferInfo {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = this->capacity(),
        .usage       = bufferUsages,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkCreateBuffer(pVkDevice->handle(), &bufferInfo, nullptr, &buffer_),
        "Failed to create buffer!");

    // Get buffer memory requirements
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(pVkDevice->handle(), buffer_, &req);

    VkMemoryAllocateInfo allocInfo {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = 0
    };

    // Alocate memory for the buffer
    RETURN_FAIL_IF_ANY_VK_FAILED(
        findMemoryTypeIndex(req.memoryTypeBits,
                            pVkDevice->physicalDeviceMemoryProperties(),
                            properties,
                            &allocInfo.memoryTypeIndex),
        "No valid memory type index found : memoryTypeBits:"
        << req.memoryTypeBits);

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkAllocateMemory(pVkDevice->handle(), &allocInfo, nullptr, &bufferMem_),
        "Failed to allocate buffer memory!");

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkBindBufferMemory(pVkDevice->handle(), buffer_, bufferMem_, 0),
        "Failed to bind buffer memory");

    return true;
}

bool StagingBuffer::create(cgl::graphics::vulkan::Device* pVkDevice) {
    LOGD("Create staging buffer(`" << name() << "`) with size:" << capacity());

    if ((pVkDevice == nullptr) || (capacity() == 0)) {
        LOGE("Invalid create args for cgl::graphics::vulkan::StagingBuffer");
        return false;
    }

    if (createNativeBuffer(
            pVkDevice,
            (VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == false) {
        LOGE("Failed to create staging buffer(`" << name() << "`) from device.");
        return false;
    }

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkMapMemory(pVkDevice->handle(),
                    bufferMem_, 0, this->capacity(),
                    0, &pMapedData_),
        "Failed to map the memory from staging buffer(`" << name() << ").");

    return true;
}

bool StagingBuffer::update(
    const void* pSrcData,
    size_t      srcDataSize,
    size_t      stagingBufferOffset
) {
    if ((pSrcData == nullptr) || (srcDataSize == 0)) {
        LOGE("Invalid source data to update data to staging buffer("
             << name() << ").");
        return false;
    }
    if ((stagingBufferOffset >= this->capacity()) ||
        (srcDataSize > (this->capacity() - stagingBufferOffset))) {
        LOGE("Source data (offset + size) is out of bounds: "
             << stagingBufferOffset << " + " << srcDataSize
             << " > " << this->capacity());
        return false;
    }

    // update data
    auto pStore = static_cast<uint8_t *>(pMapedData_) + stagingBufferOffset;
    memcpy(pStore, pSrcData, srcDataSize);

    return true;
}
