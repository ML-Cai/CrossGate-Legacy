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

using cgl::vk::StagingBuffer;

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

bool StagingBuffer::createInternal(cgl::vk::Device* pDevice) {
    LOGD("Create staging buffer(`" << name_ << "`) with size:" << capacity());

    if ((pDevice == nullptr) || (capacity() == 0)) {
        LOGE("Invalid create args for cgl::vk::StagingBuffer");
        return false;
    }

    device_ = pDevice->device();

    RETURN_FALSE_IF_ERROR(
        pDevice->createBuffer(this->capacity(),
            (VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
             VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
            &buffer_,
            &bufferMem_),
        "Failed to create staging buffer(`" << name_ << "`) from device.");

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkMapMemory(device_,
                    bufferMem_, 0, this->capacity(),
                    0, &pMapedData_),
        "Failed to map the memory from staging buffer(`" << name_ << ").");

    return true;
}

bool StagingBuffer::update(
    const void* pSrcData,
    size_t      srcDataSize,
    size_t      stagingBufferOffset
) {
    if ((pSrcData == nullptr) || (srcDataSize == 0)) {
        LOGE("Invalid source data to update data to staging buffer("
             << name_ << ").");
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
