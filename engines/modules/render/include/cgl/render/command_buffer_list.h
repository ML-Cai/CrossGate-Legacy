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
class ICommandBufferList;

class ICommandBuffer {
 public:
    using Ptr = std::unique_ptr<cgl::ICommandBuffer>;

    explicit ICommandBuffer() = default;

    virtual ~ICommandBuffer() = default;

    virtual bool reset() = 0;

    virtual bool begin() = 0;

    virtual bool end() = 0;

    virtual uint32_t idx() const noexcept = 0;
};


class ICommandBufferList {
 public:
    using Ptr = std::unique_ptr<cgl::ICommandBufferList>;

    static cgl::ICommandBufferList::Ptr create(cgl::IDevice* pDevice,
                                               uint32_t      bufferCount);

    explicit ICommandBufferList() = default;

    virtual ~ICommandBufferList() = default;

    virtual cgl::ICommandBuffer* commandBuffer(uint32_t idx) = 0;

    virtual uint32_t bufferCount() const noexcept = 0;

 protected:
    virtual bool createCommandBuffers() = 0;
};

}   // namespace cgl
