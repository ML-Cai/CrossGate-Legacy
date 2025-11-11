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
#include "cgl/graphics/staging_buffer.h"

namespace cgl {
namespace vk {

class Device;

class StagingBuffer : public cgl::IStagingBuffer {
 public:
    StagingBuffer(size_t bufferCapacity, const char* pName);

    ~StagingBuffer();

    bool update(const void* pSrcData,
                size_t      srcDataSize,
                size_t      stagingBufferOffset) override;

    size_t capacity() const noexcept override { return bufferCapacity_; }

    const char* name() const noexcept override { return name_.c_str(); }

    void* data() const noexcept override { return pMapedData_; }

    void destroy();

    bool createInternal(cgl::vk::Device* pDevice);

    VkBuffer buffer() const { return buffer_; }

 private:
    const size_t  bufferCapacity_;
    std::string     name_;
    VkDevice        device_;
    VkBuffer        buffer_;
    VkDeviceMemory  bufferMem_;
    void*           pMapedData_;

};

}   // namespace vk
}   // namespace cgl
