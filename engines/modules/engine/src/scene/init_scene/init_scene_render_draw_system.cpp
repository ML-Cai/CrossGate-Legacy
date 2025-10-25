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

using cgl::component::RenderDeviceState;
using cgl::component::SceneRenderState;
using cgl::component::WindowHandle;
using cgl::component::PrimarySceneRenderFrame;
using cgl::component::PrimaryRenderSyncObjects;

// -----------------------------------------------------------------------------
cgl::InitSceneRenderDrawSystem::InitSceneRenderDrawSystem()
    : frameIdx_(0) {
    updater_ = &InitSceneRenderDrawSystem::init;
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderDrawSystem::init(cgl::ECSCore* pECS) {
    pSceneRenderState_  = pECS->getSingleton<SceneRenderState>();
    assert(pRenderDeviceState_ != nullptr);
    if (pSceneRenderState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pSceneRenderState_, "The `InitSceneRenderDrawSystem` "
            "updates data and systems only when the `RenderState` is in the "
            "`RUNNING` state. Please verify the flow.");
        return;
    }

    // acquire objects
    pRenderDeviceState_ = pECS->getSingleton<RenderDeviceState>();
    pWindowHandle_      = pECS->getSingleton<WindowHandle>();
    pFrame_             = pECS->getSingleton<PrimarySceneRenderFrame>();
    pSyncObjs_          = pECS->getSingleton<PrimaryRenderSyncObjects>();

    if ((pRenderDeviceState_ == nullptr) || (pWindowHandle_ == nullptr) ||
        (pFrame_ == nullptr) || (pSyncObjs_ == nullptr)
    ) {
        cgl::RaiseError(pSceneRenderState_,
            "One of essential render objects is invalid.");
        return;
    }

    // check essential render pass exist
    std::vector<const char *> renderPassKey = {
        cgl::INIT_SCENE_BASIC_RENDER_PASS
    };

    for (const auto& key : renderPassKey) {
        if (pFrame_->renderPasses.find(key) == pFrame_->renderPasses.end()) {
            cgl::RaiseError(pSceneRenderState_,
                "No valid render pass: `", key, "` found in the system");
            return;
        }
    }

    // switch to render stage
    updater_ = &InitSceneRenderDrawSystem::render;
}

// -----------------------------------------------------------------------------
cgl::component::RenderSyncObjects*
cgl::InitSceneRenderDrawSystem::acquireNextSyncObj() noexcept {
    uint32_t idx = frameIdx_ % pSyncObjs_->objs.size();
    frameIdx_++;
    return &pSyncObjs_->objs[idx];
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderDrawSystem::render(cgl::ECSCore* pECS) {
    auto pSyncObj    = acquireNextSyncObj();
    auto pCmdBuffer  = pFrame_->pCmdBufferList->commandBuffer(pSyncObj->index);
    auto pRenderPass = pFrame_->renderPasses[cgl::INIT_SCENE_BASIC_RENDER_PASS].get();
    auto pSwapchain  = pRenderDeviceState_->pSwapchain.get();
    auto pDevice     = pRenderDeviceState_->pDevice.get();

    if (pSceneRenderState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pSceneRenderState_, "The `InitSceneRenderDrawSystem` "
            "updates data and systems only when the `SceneRenderState` is in "
            "the `RUNNING` state. Please verify the flow.");
        return;
    }

     // Wait fence valid
    if (pSyncObj->pFence->wait() == false) {
        cgl::RaiseError(pSceneRenderState_, "Failed to wait the fence");
        return;
    }

    // acquire image index from swapchain to choose the frame buffer.
    uint32_t imageIdx;
    if (pSwapchain->acquireNextImageIndex(
            pSyncObj->pImageAvailableSemaphore.get(),
            &imageIdx) == false) {
        cgl::RaiseError(pSceneRenderState_,
            "Failed to acquireNextImageIndex for curren frame");
        return;
    }

    // acquire frame buffer
    auto pFramebuffer = pRenderPass->acquireFramebuffer(imageIdx);
    if (pFramebuffer == nullptr) {
        cgl::RaiseError(pSceneRenderState_,
            "Failed to acquire frame buffer with index: ", imageIdx);
        return;
    }

    // reset objects
    if ((pSyncObj->pFence->reset()) == false ||
        (pCmdBuffer->reset() == false)) {
        cgl::RaiseError(pSceneRenderState_,
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
        cgl::RaiseError(pSceneRenderState_,
            "Failed to submit commands to the graphics queue");
        return;
    }

    // present
    if (pDevice->presentQueue()->present(
            pSyncObj->pRenderFinishSemaphore.get(),
            pSwapchain,
            imageIdx) == false) {
        cgl::RaiseError(pSceneRenderState_,
            "Failed to submit data to the present queue");
        return;
    }
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderDrawSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}
