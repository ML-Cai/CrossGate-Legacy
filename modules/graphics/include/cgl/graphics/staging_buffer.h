// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>

namespace cgl {

class IDevice;

// -----------------------------------------------------------------------------
class IStagingBuffer {
 public:
    using Ptr = std::unique_ptr<IStagingBuffer>;

    static IStagingBuffer::Ptr create(
        cgl::IDevice*  pDevice,
        const size_t   bufferCapacity,
        const char*    pName = "");

    IStagingBuffer() = default;

    virtual ~IStagingBuffer() = default;

    virtual bool update(const void* pSrcData,
                        size_t      srcDataSize,
                        size_t      stagingBufferOffset) = 0;

    virtual size_t capacity() const noexcept = 0;

    virtual const char* name() const noexcept = 0;

    virtual void* data() const noexcept = 0;
};


// -----------------------------------------------------------------------------
}   // namespace cgl
