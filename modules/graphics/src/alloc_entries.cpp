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

// render pass impls
#include "vulkan/render_pass/init_scene_render_pass_impl.h"
// #include "vulkan/render_pass/main_scene_render_pass_impl.h"

//------------------------------------------------------------------------------
// cgl::IDevice
//------------------------------------------------------------------------------
cgl::IDevice::Ptr cgl::IDevice::create(
    void* windowNativeHandle,
    bool enableDebug
) {
    // check input
    if (windowNativeHandle == nullptr) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::vk::Device>(enableDebug);
    if (p->prepare(windowNativeHandle)) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::ISwapchain
//------------------------------------------------------------------------------
cgl::ISwapchain::Ptr cgl::ISwapchain::create(
    cgl::IDevice* pDevice,
    void*         pWindowNativeHandle
) {
    // check input
    if ((pDevice == nullptr) || (pWindowNativeHandle == nullptr)) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::vk::Swapchain>(pDevice, pWindowNativeHandle);
    if (p->prepare(pWindowNativeHandle)) {
        return p;
    }
    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::IRenderPass
//------------------------------------------------------------------------------
cgl::IRenderPass::Ptr cgl::IRenderPass::create(
    cgl::IRenderPass::Types      type,
    cgl::IDevice*                pDevice,
    cgl::ISwapchain*             pSwapchain
) {
    // check input
    if ((pDevice == nullptr) || (pSwapchain == nullptr)) {
        LOGE("Invalid input args");
        return nullptr;
    }

    auto pNativeDevice = static_cast<cgl::vk::Device *>(pDevice);

    // create backend
    if (type == cgl::IRenderPass::Types::InitScene) {
        auto p = std::make_unique<cgl::vk::InitSceneRenderPass>(pNativeDevice->device());
        if (p->prepare(pSwapchain)) {
            return p;
        }
    }
    // else  if (type == cgl::IRenderPass::Types::MainScene) {
    //     auto p = std::make_unique<cgl::vk::MainSceneRenderPass>(createInfo);
    //     if (p->prepare()) {
    //         return p;
    //     }
    // }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::ICommandBufferList
//------------------------------------------------------------------------------
cgl::ICommandBufferList::Ptr cgl::ICommandBufferList::create(
    cgl::IDevice* pDevice,
    uint32_t      bufferCount
) {
    // check input
    if ((pDevice == nullptr) || (bufferCount == 0)) {
        return nullptr;
    }

    // create backend
    auto p = std::make_unique<cgl::vk::CommandBufferList>(pDevice, bufferCount);
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::IFence
//------------------------------------------------------------------------------
cgl::IFence::Ptr cgl::IFence::create(
    cgl::IDevice* pDevice
) {
    // check input
    if (pDevice == nullptr) {
        return nullptr;
    }

    auto pNativeDevice = static_cast<cgl::vk::Device *>(pDevice);

    // create backend
    auto p = std::make_unique<cgl::vk::Fence>(pNativeDevice->device());
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::ISemaphore
//------------------------------------------------------------------------------
cgl::ISemaphore::Ptr cgl::ISemaphore::create(
    cgl::IDevice* pDevice
) {
    // check input
    if (pDevice == nullptr) {
        return nullptr;
    }
    auto pNativeDevice = static_cast<cgl::vk::Device *>(pDevice);

    // create backend
    auto p = std::make_unique<cgl::vk::Semaphore>(pNativeDevice->device());
    if (p->prepare()) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::IFramebuffer
//------------------------------------------------------------------------------
cgl::IFramebuffer::Ptr cgl::IFramebuffer::create(
    cgl::IDevice*     pDevice,
    cgl::IRenderPass* pRenderpass
) {
    // check input
    if ((pDevice == nullptr) || (pRenderpass == nullptr)) {
        return nullptr;
    }
    auto pNativeDevice = static_cast<cgl::vk::Device *>(pDevice);

    // create backend
    return std::make_unique<cgl::vk::Framebuffer>(pNativeDevice->device());
}

//------------------------------------------------------------------------------
// cgl::IBuffer
//------------------------------------------------------------------------------
cgl::IBuffer::Ptr cgl::IBuffer::create(
    cgl::IDevice*       pDevice,
    cgl::IBuffer::Types type,
    size_t              bufferCapacity,
    const char*         pName
) {
    auto pVkDevice = static_cast<cgl::vk::Device *>(pDevice);
    auto p = std::make_unique<cgl::vk::Buffer>(type, bufferCapacity, pName);
    if ((p != nullptr) && (p->createInternal(pVkDevice) == true)) {
        return p;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
// cgl::IStagingBuffer
//------------------------------------------------------------------------------
cgl::IStagingBuffer::Ptr cgl::IStagingBuffer::create(
    cgl::IDevice* pDevice,
    size_t        bufferCapacity,
    const char*   pName
) {
    auto pVkDevice = static_cast<cgl::vk::Device *>(pDevice);
    auto p = std::make_unique<cgl::vk::StagingBuffer>(bufferCapacity, pName);
    if ((p != nullptr) && (p->createInternal(pVkDevice) == true)) {
        return p;
    }

    return nullptr;
}

// //------------------------------------------------------------------------------
// // cgl::IGridLineRenderer
// //------------------------------------------------------------------------------
// cgl::IGridLineRenderer::Ptr cgl::IGridLineRenderer::create(
//     cgl::IDevice*     pDevice,
//     cgl::IRenderPass* pRenderPass
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pRenderPass == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::IGridLineRenderer::CreateInfo createInfo {
//         .pDevice     = pDevice,
//         .pRenderPass = pRenderPass,
//     };

//     auto p = cgl::CreateGridLineRendererImpl(createInfo);
//     if ((p != nullptr) && (p->init() == true) && (p->initBackend() == true)) {
//         return p;
//     }

//     return nullptr;
// }

// //------------------------------------------------------------------------------
// // cgl::IMapRenderer
// //------------------------------------------------------------------------------
// cgl::IMapRenderer::Ptr cgl::IMapRenderer::create(
//     cgl::IDevice*     pDevice,
//     cgl::IRenderPass* pRenderPass
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pRenderPass == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::IMapRenderer::CreateInfo createInfo {
//         .pDevice     = pDevice,
//         .pRenderPass = pRenderPass,
//     };

//     auto p = cgl::CreateMapRendererImpl(createInfo);
//     if ((p != nullptr) && (p->init() == true) && (p->initBackend() == true)) {
//         return p;
//     }

//     return nullptr;
// }

// //------------------------------------------------------------------------------
// // cgl::IFlatImageRenderer
// //------------------------------------------------------------------------------
// cgl::IFlatImageRenderer::Ptr cgl::IFlatImageRenderer::create(
//     cgl::IDevice*        pDevice,
//     cgl::IRenderPass*    pRenderPass,
//     cgl::ImageResourceID imgResourceId
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pRenderPass == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::IFlatImageRenderer::CreateInfo createInfo {
//         .pDevice       = pDevice,
//         .pRenderPass   = pRenderPass,
//         .imgResourceId = imgResourceId
//     };

//     auto p = cgl::CreateFlatImageRendererImpl(createInfo);
//     if ((p != nullptr) && (p->init() == true) && (p->initBackend() == true)) {
//         return p;
//     }

//     return nullptr;
// }

// //------------------------------------------------------------------------------
// // cgl::ITextureObject
// //------------------------------------------------------------------------------
// cgl::ITextureObject::Ptr cgl::ITextureObject::create(
//     cgl::IDevice*            pDevice,
//     cgl::IFileImageResource* pFileImgRes
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pFileImgRes == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::ITextureObject::CreateInfo createInfo {
//         .type        = cgl::ITextureObject::Types::Flat2DImage,
//         .pDevice     = pDevice,
//         .pFileImgRes = pFileImgRes,
//     };

//     auto p = std::make_shared<cgl::vk::ImageObjectImpl>(createInfo);
//     if (p->prepare(pFileImgRes)) {
//         return p;
//     }

//     return nullptr;
// }

// cgl::ITextureObject::Ptr cgl::ITextureObject::create(
//     cgl::IDevice*               pDevice,
//     const cgl::MapTextureAtlas* pMapTextureAtlas
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pMapTextureAtlas == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::ITextureObject::CreateInfo createInfo {
//         .type             = cgl::ITextureObject::Types::Flat2DImage,
//         .pDevice          = pDevice,
//         .pMapTextureAtlas = pMapTextureAtlas,
//     };

//     auto p = std::make_shared<cgl::vk::ImageObjectImpl>(createInfo);
//     if (p->prepare(pMapTextureAtlas)) {
//         return p;
//     }

//     return nullptr;
// }


// cgl::ITextureObject::Ptr cgl::ITextureObject::create(
//     cgl::IDevice*            pDevice,
//     const cgl::PaletteAtlas* pPaletteAtlas
// ) {
//     // check input
//     if ((pDevice == nullptr) || (pPaletteAtlas == nullptr)) {
//         return nullptr;
//     }

//     // create backend
//     cgl::ITextureObject::CreateInfo createInfo {
//         .type          = cgl::ITextureObject::Types::Flat2DImage,
//         .pDevice       = pDevice,
//         .pPaletteAtlas = pPaletteAtlas,
//     };

//     auto p = std::make_shared<cgl::vk::ImageObjectImpl>(createInfo);
//     if (p->prepare(pPaletteAtlas)) {
//         return p;
//     }

//     return nullptr;
// }
