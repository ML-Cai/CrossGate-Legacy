// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include "cgl/graphics/queue.h"

namespace cgl {
namespace vk {

class Queue : public cgl::IQueue {
 public:
    explicit Queue(cgl::IQueue::Type type,
                   VkDevice device,
                   VkQueue queue);

    ~Queue();

    bool submit(cgl::ICommandBuffer* pCmdBuffers,
                cgl::IFence*         pFence) override;

    bool submit(cgl::ICommandBuffer* pCmdBuffers,
                cgl::ISemaphore*     pWaitSems,
                cgl::ISemaphore*     pSignalSems,
                cgl::IFence*         pFence) override;

    bool present(cgl::ISemaphore* pWaitSems,
                 cgl::ISwapchain* pSwapchain,
                 uint32_t         imageIdx) override;

    VkQueue queue() const noexcept { return queue_; }

 private:
    cgl::IQueue::Type queueType_;
    VkDevice device_;
    VkQueue queue_;
};

}   // namespace vk
}   // namespace cgl
