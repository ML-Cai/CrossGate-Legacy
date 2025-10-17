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
class IFence;
class ISemaphore;
class ICommandBuffer;
class ISwapchain;

class IQueue {
 public:
    using Ptr = std::unique_ptr<cgl::IQueue>;

    enum class Type {
        PresentQueue,
        GraphicsQueue,
    };

    static cgl::IQueue::Ptr create(cgl::IDevice* pDevice,
                                   cgl::IQueue::Type type);

    IQueue() = default;

    virtual ~IQueue() = default;

    virtual bool submit(
        cgl::ICommandBuffer* pCmdBuffers,
        cgl::ISemaphore*     pWaitSems,
        cgl::ISemaphore*     pSignalSems,
        cgl::IFence*         pFence) = 0;

    virtual bool present(
        cgl::ISemaphore* pWaitSems,
        cgl::ISwapchain* pSwapchain,
        uint32_t         imageIdx) = 0;
};

}   // namespace cgl
