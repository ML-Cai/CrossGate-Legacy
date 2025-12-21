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
namespace graphics {

class IDevice;
class ISemaphore;
class ICommandBuffer;
class IFence;
class ISwapchain;

class IQueue {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::IQueue>;

    enum class Type {
        PresentQueue,
        GraphicsQueue,
    };

    static cgl::graphics::IQueue::Ptr create(
        cgl::graphics::IDevice*     pDevice,
        cgl::graphics::IQueue::Type type);

    IQueue() = default;

    virtual ~IQueue() = default;

    virtual bool submit(
        cgl::graphics::ICommandBuffer* pCmdBuffers,
        cgl::graphics::IFence*         pFence) = 0;

    virtual bool submit(
        cgl::graphics::ICommandBuffer* pCmdBuffers,
        cgl::graphics::ISemaphore*     pWaitSems,
        cgl::graphics::ISemaphore*     pSignalSems,
        cgl::graphics::IFence*         pFence) = 0;

    virtual bool present(
        cgl::graphics::ISemaphore* pWaitSems,
        cgl::graphics::ISwapchain* pSwapchain,
        uint32_t                   imageIdx) = 0;
};

}   // namespace graphics
}   // namespace cgl
