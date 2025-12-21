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
namespace graphics {
namespace vulkan {

class Queue : public cgl::graphics::IQueue {
 public:
    explicit Queue(cgl::graphics::IQueue::Type type,
                   VkDevice device,
                   VkQueue queue);

    ~Queue();

    bool submit(cgl::graphics::ICommandBuffer* pCmdBuffers,
                cgl::graphics::IFence*         pFence) override;

    bool submit(cgl::graphics::ICommandBuffer* pCmdBuffers,
                cgl::graphics::ISemaphore*     pWaitSems,
                cgl::graphics::ISemaphore*     pSignalSems,
                cgl::graphics::IFence*         pFence) override;

    bool present(cgl::graphics::ISemaphore* pWaitSems,
                 cgl::graphics::ISwapchain* pSwapchain,
                 uint32_t                   imageIdx) override;

    VkQueue queue() const noexcept { return queue_; }

 private:
    cgl::graphics::IQueue::Type queueType_;
    VkDevice device_;
    VkQueue queue_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
