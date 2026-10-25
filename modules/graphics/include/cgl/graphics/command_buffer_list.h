// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include <optional>

namespace cgl {
namespace graphics {

class IDevice;
class ICommandBufferList;

class ICommandBuffer {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::ICommandBuffer>;

    explicit ICommandBuffer() = default;

    virtual ~ICommandBuffer() = default;

    virtual bool reset() = 0;

    virtual bool begin() = 0;

    virtual bool end() = 0;

    virtual uint32_t idx() const noexcept = 0;

    // Optional diagnostics. Enable only at an idle lifetime boundary, before
    // recording. Unsupported backends/devices return false without allocating.
    virtual bool enableGpuTiming() {return false;}
    virtual bool gpuTimingEnabled() const noexcept {return false;}
    // Caller MUST establish completion of this recording's successful submission
    // (fence/queue idle), then consume before reset. Never polls an in-flight or
    // merely recorded/unsubmitted buffer. No wait is added; a result is taken once.
    virtual std::optional<double> takeGpuTimeAfterCompletion() {return std::nullopt;}
};


class ICommandBufferList {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::ICommandBufferList>;

    static cgl::graphics::ICommandBufferList::Ptr create(
                cgl::graphics::IDevice* pDevice,
                uint32_t                bufferCount);

    explicit ICommandBufferList() = default;

    virtual ~ICommandBufferList() = default;

    virtual cgl::graphics::ICommandBuffer* commandBuffer(uint32_t idx) = 0;

    virtual uint32_t bufferCount() const noexcept = 0;

 protected:
    virtual bool createCommandBuffers() = 0;
};

}   // namespace graphics
}   // namespace cgl
