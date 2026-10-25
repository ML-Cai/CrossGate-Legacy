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
#include "cgl/graphics/queue.h"
#include "render/render_components.h"
#include "render/frame_render_systems.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/engine_components.h"
#include "telemetry/frame_telemetry.h"
#include <chrono>

using cgl::component::RenderDeviceState;
using cgl::component::FrameRenderState;
using cgl::component::WindowHandle;
using cgl::component::PrimarySceneRenderFrame;
using cgl::component::PrimaryRenderSyncObjects;

// -----------------------------------------------------------------------------
cgl::FrameRenderDrawSystem::FrameRenderDrawSystem()
    : frameIdx_(0) {
    updater_ = &FrameRenderDrawSystem::init;
}

// -----------------------------------------------------------------------------
void cgl::FrameRenderDrawSystem::init(cgl::ECSCore* pECS) {
    pFrameRenderState_  = pECS->getSingleton<FrameRenderState>();
    assert(pFrameRenderState_ != nullptr);
    if (pFrameRenderState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pFrameRenderState_, "The `FrameRenderDrawSystem` "
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
        cgl::RaiseError(pFrameRenderState_,
            "One of essential render objects is invalid.");
        return;
    }

    // check essential render pass exist
    std::vector<const char *> renderPassKey = {
        cgl::PRIMARY_RENDER_PASS
    };

    for (const auto& key : renderPassKey) {
        if (pFrame_->renderPasses.find(key) == pFrame_->renderPasses.end()) {
            cgl::RaiseError(pFrameRenderState_,
                "No valid render pass: `", key, "` found in the system");
            return;
        }
    }

    if(auto* telemetry=pECS->getSingleton<cgl::component::RuntimeTelemetry>()) {
        for(uint32_t i=0;i<pFrame_->pCmdBufferList->bufferCount();++i)
            if(pFrame_->pCmdBufferList->commandBuffer(i)->enableGpuTiming()) ++telemetry->gpuSlots;
    }
    // switch to render stage
    updater_ = &FrameRenderDrawSystem::render;
}

// -----------------------------------------------------------------------------
cgl::component::RenderSyncObjects*
cgl::FrameRenderDrawSystem::acquireNextSyncObj() noexcept {
    uint32_t idx = frameIdx_ % pSyncObjs_->objs.size();
    frameIdx_++;
    return &pSyncObjs_->objs[idx];
}

// -----------------------------------------------------------------------------
void cgl::FrameRenderDrawSystem::render(cgl::ECSCore* pECS) {
    auto pSyncObj    = acquireNextSyncObj();
    auto pCmdBuffer  = pFrame_->pCmdBufferList->commandBuffer(pSyncObj->index);
    auto pRenderPass = pFrame_->renderPasses[cgl::PRIMARY_RENDER_PASS].get();
    auto pSwapchain  = pRenderDeviceState_->pSwapchain.get();
    auto pDevice     = pRenderDeviceState_->pDevice.get();

    if (pFrameRenderState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pFrameRenderState_, "The `FrameRenderDrawSystem` "
            "updates data and systems only when the `FrameRenderState` is in "
            "the `RUNNING` state. Please verify the flow.");
        return;
    }

    auto* telemetry=pECS->getSingleton<cgl::component::RuntimeTelemetry>();
    using Clock=std::chrono::steady_clock;
    const auto waitStart=telemetry?Clock::now():Clock::time_point{};
     // Wait fence valid
    if (pSyncObj->pFence->wait() == false) {
        cgl::RaiseError(pFrameRenderState_, "Failed to wait the fence");
        return;
    }

    if(telemetry) telemetry->rendering.fenceWaitMs=std::chrono::duration<double,std::milli>(Clock::now()-waitStart).count();
    // Read only this slot's completed, successful recording, before fence/reset reuse.
    if(pSyncObj->submissionSerial) {
        const auto gpu=pCmdBuffer->takeGpuTimeAfterCompletion();
        if(telemetry && pSyncObj->gpuProfiled) telemetry->gpu.add(gpu);
        pSyncObj->gpuProfiled=false;
    }
    pSyncObjs_->progress.didComplete(pSyncObj->submissionSerial);

    // acquire image index from swapchain to choose the frame buffer.
    uint32_t imageIdx;
    if (pSwapchain->acquireNextImageIndex(
            pSyncObj->pImageAvailableSemaphore.get(),
            &imageIdx) == false) {
        cgl::RaiseError(pFrameRenderState_,
            "Failed to acquireNextImageIndex for curren frame");
        return;
    }

    // acquire frame buffer
    auto pFramebuffer = pRenderPass->acquireFramebuffer(imageIdx);
    if (pFramebuffer == nullptr) {
        cgl::RaiseError(pFrameRenderState_,
            "Failed to acquire frame buffer with index: ", imageIdx);
        return;
    }

    const auto recordStart=telemetry?Clock::now():Clock::time_point{};
    // reset objects
    if ((pSyncObj->pFence->reset()) == false ||
        (pCmdBuffer->reset() == false)) {
        cgl::RaiseError(pFrameRenderState_,
            "Failed to reset the objects for rednering");
        return;
    }

    // prepare command buffer
    cgl::Viewport viewport(0, 0, pWindowHandle_->width, pWindowHandle_->height);
    if (!pCmdBuffer->begin()) {
        cgl::RaiseError(pFrameRenderState_, "Failed to begin command buffer");
        return;
    }
    {
        if (!pRenderPass->begin(viewport, pCmdBuffer, pFramebuffer)) {
            cgl::RaiseError(pFrameRenderState_, "Failed to begin render pass");
            return;
        }
        auto* scene = pECS->getSingleton<cgl::component::SceneState>();
        if (scene->scene) scene->scene->draw(pECS, pCmdBuffer, viewport);
        if (!pRenderPass->end()) {
            cgl::RaiseError(pFrameRenderState_, "Failed to end render pass");
            return;
        }
    }
    if (!pCmdBuffer->end()) {
        cgl::RaiseError(pFrameRenderState_, "Failed to end command buffer");
        return;
    }

    if(telemetry) telemetry->rendering.recordMs=std::chrono::duration<double,std::milli>(Clock::now()-recordStart).count();
    // submit the rendering
    if (pDevice->graphicsQueue()->submit(
            pCmdBuffer,
            pSyncObj->pImageAvailableSemaphore.get(),
            pSyncObjs_->presentSemaphores[imageIdx].get(),
            pSyncObj->pFence.get()) == false) {
        cgl::RaiseError(pFrameRenderState_,
            "Failed to submit commands to the graphics queue");
        return;
    }

    pSyncObj->submissionSerial=pSyncObjs_->progress.didSubmit();
    const auto* scene=pECS->getSingleton<cgl::component::SceneState>();
    const auto* loading=pECS->getSingleton<cgl::component::SceneLoadProgress>();
    pSyncObj->gpuProfiled=telemetry && pCmdBuffer->gpuTimingEnabled() && scene->scene &&
        scene->scene->sceneType()!=cgl::SceneTypes::InitScene && (!loading || loading->ready);

    // present
    if (pDevice->presentQueue()->present(
            pSyncObjs_->presentSemaphores[imageIdx].get(),
            pSwapchain,
            imageIdx) == false) {
        cgl::RaiseError(pFrameRenderState_,
            "Failed to submit data to the present queue");
        return;
    }
}

// -----------------------------------------------------------------------------
void cgl::FrameRenderDrawSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}
