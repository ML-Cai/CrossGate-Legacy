// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#include "cgl/trace/logger.h"
#include "vulkan/device.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/semaphore.h"
#include "vulkan/fence.h"
#include "vulkan/framebuffer.h"
#include "vulkan/buffer.h"
#include "vulkan/staging_buffer.h"
#include "vulkan/buffer_allocator.h"

// render pass impls
#include "vulkan/render_pass/init_scene_render_pass_impl.h"
// #include "vulkan/render_pass/main_scene_render_pass_impl.h"

//------------------------------------------------------------------------------
// cgl::graphics::IDevice
//------------------------------------------------------------------------------
cgl::graphics::IDevice::Ptr cgl::graphics::IDevice::create(
    void* windowNativeHandle,
    bool enableDebug
) {
    // check input
    if (windowNativeHandle == nullptr) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::graphics::vulkan::Device>(enableDebug);
    if (p->prepare(windowNativeHandle)) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::ISwapchain
//------------------------------------------------------------------------------
cgl::graphics::ISwapchain::Ptr
cgl::graphics::ISwapchain::create(
    cgl::graphics::IDevice* pDevice,
    void*                   pWindowNativeHandle
) {
    // check input
    if ((pDevice == nullptr) || (pWindowNativeHandle == nullptr)) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::graphics::vulkan::Swapchain>(
                pDevice, pWindowNativeHandle);
    if (p->prepare(pWindowNativeHandle)) {
        return p;
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::IRenderPass
//------------------------------------------------------------------------------
cgl::graphics::IRenderPass::Ptr cgl::graphics::IRenderPass::create(
    cgl::graphics::IRenderPass::Types type,
    cgl::graphics::IDevice*           pDevice,
    cgl::graphics::ISwapchain*        pSwapchain
) {
    // check input
    if ((pDevice == nullptr) || (pSwapchain == nullptr)) {
        LOGE("Invalid input args");
        return nullptr;
    }

    auto pNativeDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);

    // create backend
    if (type == cgl::graphics::IRenderPass::Types::InitScene) {
        auto p = std::make_unique<cgl::graphics::vulkan::InitSceneRenderPass>(
                    pNativeDevice->device());
        if (p->prepare(pSwapchain)) {
            return p;
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::ICommandBufferList
//------------------------------------------------------------------------------
cgl::graphics::ICommandBufferList::Ptr
cgl::graphics::ICommandBufferList::create(
    cgl::graphics::IDevice* pDevice,
    uint32_t                bufferCount
) {
    // check input
    if ((pDevice == nullptr) || (bufferCount == 0)) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::graphics::vulkan::CommandBufferList>(
                pDevice, bufferCount);
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::IFence
//------------------------------------------------------------------------------
cgl::graphics::IFence::Ptr cgl::graphics::IFence::create(
    cgl::graphics::IDevice* pDevice
) {
    // check input
    if (pDevice == nullptr) {
        return nullptr;
    }

    auto pNativeDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);

    // create backend
    auto p = std::make_unique<cgl::graphics::vulkan::Fence>(pNativeDevice->device());
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::ISemaphore
//------------------------------------------------------------------------------
cgl::graphics::ISemaphore::Ptr
cgl::graphics::ISemaphore::create(
    cgl::graphics::IDevice* pDevice
) {
    // check input
    if (pDevice == nullptr) {
        return nullptr;
    }
    auto pNativeDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);

    // create backend
    auto p = std::make_unique<cgl::graphics::vulkan::Semaphore>(
                pNativeDevice->device());
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::graphics::IFramebuffer
//------------------------------------------------------------------------------
cgl::graphics::IFramebuffer::Ptr
cgl::graphics::IFramebuffer::create(
    cgl::graphics::IDevice*     pDevice,
    cgl::graphics::IRenderPass* pRenderpass
) {
    // check input
    if ((pDevice == nullptr) || (pRenderpass == nullptr)) {
        return nullptr;
    }
    auto pNativeDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);

    // create backend
    return std::make_unique<cgl::graphics::vulkan::Framebuffer>(
                pNativeDevice->device());
}

//------------------------------------------------------------------------------
// cgl::graphics::IStagingBuffer
//------------------------------------------------------------------------------
cgl::graphics::IStagingBuffer::Ptr
cgl::graphics::IStagingBuffer::create(
    cgl::graphics::IDevice* pDevice,
    size_t                  bufferCapacity,
    const char*             pName
) {
    auto pVkDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);
    auto p = std::make_unique<cgl::graphics::vulkan::StagingBuffer>(
                bufferCapacity, pName);
    if ((p != nullptr) && (p->create(pVkDevice) == true)) {
        return p;
    }

    return nullptr;
}

// -----------------------------------------------------------------------------
// cgl::IBufferAllocator::create
// -----------------------------------------------------------------------------
cgl::graphics::IBufferAllocator::Ptr cgl::graphics::IBufferAllocator::create(
    cgl::graphics::IBuffer::Types bufferType,
    cgl::graphics::IDevice*       pDevice,
    size_t                        poolSize
) {
    auto pVkDevice = static_cast<cgl::graphics::vulkan::Device *>(pDevice);
    return cgl::graphics::vulkan::BufferAllocator::create(
                bufferType, pVkDevice, poolSize);
}
