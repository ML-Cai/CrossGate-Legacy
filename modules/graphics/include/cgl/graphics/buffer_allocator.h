// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/graphics/buffer.h"

namespace cgl {
namespace graphics {

class IDevice;

// -----------------------------------------------------------------------------
class IBufferAllocator {
 public:
    using Ptr = std::unique_ptr<IBufferAllocator>;

    static IBufferAllocator::Ptr create(
        cgl::graphics::IBuffer::Types type,
        cgl::graphics::IDevice*       pDevice,
        size_t                        poolSize = 16 * 1024 * 1024);

    IBufferAllocator() = default;

    virtual ~IBufferAllocator() = default;

    virtual cgl::graphics::IBuffer::Ptr alloc(size_t size,
                                              const char* pName = "") = 0;

    virtual cgl::graphics::IBuffer::Types bufferType() const noexcept = 0;

    virtual size_t poolSize() const noexcept = 0;
};

// -----------------------------------------------------------------------------

}   // namespace graphics
}   // namespace cgl
