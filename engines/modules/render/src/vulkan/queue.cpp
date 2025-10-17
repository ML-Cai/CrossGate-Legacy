// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "vulkan/fence.h"
#include "vulkan/queue.h"
#include "vulkan/semaphore.h"
#include "vulkan/swapchain.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/common.h"

using cgl::vk::Queue;

// -----------------------------------------------------------------------------
// cgl::vk::Queue
// -----------------------------------------------------------------------------
Queue::Queue(cgl::IQueue::Type type, VkDevice device, VkQueue queue)
    : queueType_(type), device_(device), queue_(queue) {
}

Queue::~Queue() {
}

bool Queue::submit(
    cgl::ICommandBuffer* pCmdBuffers,
    cgl::ISemaphore*     pWaitSems,
    cgl::ISemaphore*     pSignalSems,
    cgl::IFence*         pFence
) {
    VkCommandBuffer pVkCmdBuffers[] = {
        static_cast<cgl::vk::CommandBuffer*>(pCmdBuffers)->commandBuffer()
    };

    VkSemaphore pVkSignalSems[] = {
        static_cast<cgl::vk::Semaphore*>(pSignalSems)->semaphore()
    };

    VkSemaphore pVkWaitSems[] = {
        static_cast<cgl::vk::Semaphore*>(pWaitSems)->semaphore()
    };

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submitInfo {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = pVkWaitSems,
        .pWaitDstStageMask    = waitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = pVkCmdBuffers,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = pVkSignalSems,
    };

    auto pVkFence = static_cast<cgl::vk::Fence *>(pFence);
    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkQueueSubmit(queue_, 1, &submitInfo, pVkFence->fence()),
        "Failed to submit data to the queue");

    return true;
}

bool Queue::present(
    cgl::ISemaphore* pWaitSems,
    cgl::ISwapchain* pSwapchain,
    uint32_t         imageIdx
) {
    VkSemaphore pVkWaitSems[] = {
        static_cast<cgl::vk::Semaphore*>(pWaitSems)->semaphore()
    };

    auto pVkSwapchain = static_cast<cgl::vk::Swapchain *>(pSwapchain);
    VkSwapchainKHR swapChains[] = {pVkSwapchain->swapChain()};
    VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = pVkWaitSems,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = &imageIdx,
    };

    RETURN_FAIL_IF_ANY_VK_FAILED(
        vkQueuePresentKHR(queue_, &presentInfo),
        "Failed to present the queue");

    return true;
}
