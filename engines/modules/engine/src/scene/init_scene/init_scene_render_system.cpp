// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "assert.h"
#include <memory>
#include "cgl/trace/logger.h"
#include "cgl/render/queue.h"
#include "render/render_components.h"
#include "scene/init_scene/init_scene_systems.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/engine_components.h"

using cgl::component::PrimaryDeviceContext;
using cgl::component::PrimarySwapchain;
using cgl::component::PrimarySceneRenderFrame;
using cgl::component::PrimaryRenderSyncObjects;

// -----------------------------------------------------------------------------
cgl::InitSceneRenderSystem::InitSceneRenderSystem()
    : frameIdx_(0) {
    updater_ = &InitSceneRenderSystem::init;
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderSystem::init(cgl::ECSCore* pECS) {
    pState_        = pECS->getSingleton<cgl::component::SceneState>();
    pWindowHandle_ = pECS->getSingleton<cgl::component::WindowHandle>();
    pDevCtx_       = pECS->getSingleton<PrimaryDeviceContext>();
    pSwapchain_    = pECS->getSingleton<PrimarySwapchain>();
    pFrame_        = pECS->getSingleton<PrimarySceneRenderFrame>();
    pSyncObjs_     = pECS->getSingleton<PrimaryRenderSyncObjects>();

    assert(pState_ != nullptr);

    if ((pWindowHandle_ == nullptr) ||
        (pDevCtx_ == nullptr) ||
        (pSwapchain_ == nullptr) ||
        (pFrame_ == nullptr) ||
        (pSyncObjs_ == nullptr)
    ) {
        cgl::RaiseError(pState_,
            "One of essential render objects is invalid.");
        return;
    }

    // check essential render pass exist
    std::vector<const char *> renderPassKey = {
        cgl::INIT_SCENE_BASIC_RENDER_PASS
    };

    for (const auto& key : renderPassKey) {
        if (pFrame_->renderPasses.find(key) == pFrame_->renderPasses.end()) {
            cgl::RaiseError(pState_,
                "No valid render pass: `", key, "` found in the system");
            return;
        }
    }

    // switch to render stage
    updater_ = &InitSceneRenderSystem::render;
    pState_->state = cgl::StateTypes::ACTIVE;
}

// -----------------------------------------------------------------------------
cgl::component::RenderSyncObjects*
cgl::InitSceneRenderSystem::acquireNextSyncObj() noexcept {
    uint32_t idx = frameIdx_ % pSyncObjs_->objs.size();
    frameIdx_++;
    return &pSyncObjs_->objs[idx];
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderSystem::render(cgl::ECSCore* pECS) {
    auto pSyncObj    = acquireNextSyncObj();
    auto pCmdBuffer  = pFrame_->pCmdBufferList->commandBuffer(pSyncObj->index);
    auto pRenderPass = pFrame_->renderPasses[cgl::INIT_SCENE_BASIC_RENDER_PASS].get();
    auto pSwapchain  = pSwapchain_->pSwapchain.get();
    auto pDevice     = pDevCtx_->pDevice.get();

     // Wait fence valid
    if (pSyncObj->pFence->wait() == false) {
        cgl::RaiseError(pState_, "Failed to wait the fence");
        return;
    }

    // acquire image index from swapchain to choose the frame buffer.
    uint32_t imageIdx;
    if (pSwapchain->acquireNextImageIndex(
            pSyncObj->pImageAvailableSemaphore.get(),
            &imageIdx) == false) {
        cgl::RaiseError(pState_,
            "Failed to acquireNextImageIndex for curren frame");
        return;
    }

    // acquire frame buffer
    auto pFramebuffer = pRenderPass->acquireFramebuffer(imageIdx);
    if (pFramebuffer == nullptr) {
        cgl::RaiseError(pState_,
            "Failed to acquire frame buffer with index: ", imageIdx);
        return;
    }

    // reset objects
    if ((pSyncObj->pFence->reset()) == false ||
        (pCmdBuffer->reset() == false)) {
        cgl::RaiseError(pState_,
            "Failed to reset the objects for rednering");
        return;
    }

    // prepare command buffer
    cgl::Viewport viewport(0, 0, pWindowHandle_->width, pWindowHandle_->height);
    pCmdBuffer->begin();
    {
        pRenderPass->begin(viewport, pCmdBuffer, pFramebuffer);
        pRenderPass->end();
    }
    pCmdBuffer->end();

    // submit the rendering
    if (pDevice->graphicsQueue()->submit(
            pCmdBuffer,
            pSyncObj->pImageAvailableSemaphore.get(),
            pSyncObj->pRenderFinishSemaphore.get(),
            pSyncObj->pFence.get()) == false) {
        cgl::RaiseError(pState_,
            "Failed to submit commands to the graphics queue");
        return;
    }

    // present
    if (pDevice->presentQueue()->present(
            pSyncObj->pRenderFinishSemaphore.get(),
            pSwapchain,
            imageIdx) == false) {
        cgl::RaiseError(pState_, "Failed to submit data to the present queue");
        return;
    }
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}
