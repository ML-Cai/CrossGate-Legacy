// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <memory>
#include "cgl/trace/logger.h"
#include "render/render_components.h"
#include "scene/init_scene/init_scene_systems.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "scene/scene_components.h"

namespace {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

}   // namespace

using cgl::component::RenderDeviceState;

// -----------------------------------------------------------------------------
bool cgl::InitSceneRenderInitSystem::initEssentialRenderObjects(
    cgl::ECSCore* pECS
) {
    auto pRenderDeviceState = pECS->getSingleton<RenderDeviceState>();
    auto pDevice            = pRenderDeviceState->pDevice.get();
    auto pSwapchain         = pRenderDeviceState->pSwapchain.get();
    if (pDevice == nullptr || pSwapchain == nullptr) {
        LOGE("No primary device or swapchian object created before scene init");
        return false;
    }

    // create command buffer
    auto pCmdBufferList = cgl::graphics::ICommandBufferList::create(
                            pDevice,
                            pSwapchain->imageCount());
    if (pCmdBufferList == nullptr) {
        return false;
    }

    // create render pass
    auto pRenderpass = cgl::graphics::IRenderPass::create(
                        cgl::graphics::IRenderPass::BuiltinKeys::InitScene,
                        pDevice,
                        pSwapchain);
    if (pRenderpass == nullptr) {
        return false;
    }

    std::unordered_map<std::string, cgl::graphics::IRenderPass::Ptr> renderPasses;
    renderPasses[cgl::INIT_SCENE_BASIC_RENDER_PASS] = std::move(pRenderpass);

    // create sync objects
    std::vector<cgl::component::RenderSyncObjects> syncObjects;
    syncObjects.reserve(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i++) {
        LOGD("Allocate sync object for [" << i << "/"  << MAX_FRAMES_IN_FLIGHT
             << "] frame.");

        syncObjects.emplace_back(i,
                                 std::move(cgl::graphics::ISemaphore::create(pDevice)),
                                 std::move(cgl::graphics::ISemaphore::create(pDevice)),
                                 std::move(cgl::graphics::IFence::create(pDevice)));
        if ((syncObjects.back().pImageAvailableSemaphore == nullptr) ||
            (syncObjects.back().pRenderFinishSemaphore == nullptr) ||
            (syncObjects.back().pFence == nullptr)) {
            return false;
        }
    }

    // append singleton obj
    pECS->addSingleton<cgl::component::PrimarySceneRenderFrame>(
        std::move(pCmdBufferList),
        std::move(renderPasses)
    );
    pECS->addSingleton<cgl::component::PrimaryRenderSyncObjects>(
        std::move(syncObjects)
    );

    return true;
}

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderInitSystem::update(cgl::ECSCore* pECS) {
    auto pState = pECS->getSingleton<cgl::component::SceneRenderState>();
    assert(pState != nullptr);

    if ((pState->state != cgl::StateTypes::UNKNOWN) &&
        (pState->state != cgl::StateTypes::INITIALIZING)) {
        cgl::RaiseError(pState, "The `InitSceneRenderInitSystem` updates data "
            "and systems only when the `SceneRenderState` is in the `UNKNOWN` "
            "or `INITIALIZING` state. Please verify the flow.");
        return;
    }

    if (initEssentialRenderObjects(pECS) == false) {
        cgl::RaiseError(pState,
            "Failed to init one of essential render objects in "
            "InitSceneRenderInitSystem");
        return;
    }
}
