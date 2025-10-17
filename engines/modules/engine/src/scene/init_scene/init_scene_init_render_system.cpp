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

using cgl::component::PrimaryDeviceContext;
using cgl::component::PrimarySwapchain;

// -----------------------------------------------------------------------------
bool cgl::InitSceneInitRenderSystem::initEssentialRenderObjects(
    cgl::ECSCore* pECS
) {
    auto pDevCtx        = pECS->getSingleton<PrimaryDeviceContext>();
    auto pPrimSwapchain = pECS->getSingleton<PrimarySwapchain>();
    if (pDevCtx == nullptr || pPrimSwapchain == nullptr) {
        LOGE("No primary device or swapchian object created before scene init");
        return false;
    }

    auto pDevice = pDevCtx->pDevice.get();
    auto pSwapchain = pPrimSwapchain->pSwapchain.get();

    // create command buffer
    auto pCmdBufferList = cgl::ICommandBufferList::create(
                            pDevice,
                            pSwapchain->imageCount());
    if (pCmdBufferList == nullptr) {
        return false;
    }

    // create render pass
    auto pRenderpass = cgl::IRenderPass::create(
                        cgl::IRenderPass::Types::InitScene,
                        pDevice,
                        pSwapchain);
    if (pRenderpass == nullptr) {
        return false;
    }

    std::unordered_map<std::string, cgl::IRenderPass::Ptr> renderPasses;
    renderPasses[cgl::INIT_SCENE_BASIC_RENDER_PASS] = std::move(pRenderpass);

    // create sync objects
    std::vector<cgl::component::RenderSyncObjects> syncObjects;
    syncObjects.reserve(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0 ; i < MAX_FRAMES_IN_FLIGHT ; i++) {
        LOGD("Allocate sync object for [" << i << "/"  << MAX_FRAMES_IN_FLIGHT
             << "] frame.");

        syncObjects.emplace_back(i,
                                 std::move(cgl::ISemaphore::create(pDevice)),
                                 std::move(cgl::ISemaphore::create(pDevice)),
                                 std::move(cgl::IFence::create(pDevice)));
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
void cgl::InitSceneInitRenderSystem::update(cgl::ECSCore* pECS) {
    LOGD("InitSceneInitRenderSystem::update");

    pState_ = pECS->getSingleton<cgl::component::SceneState>();
    pState_->state = cgl::StateTypes::INITIALIZING;

    if (initEssentialRenderObjects(pECS) == false) {
        cgl::RaiseError(pState_,
            "Failed to init essential render objects in "
            "InitSceneInitRenderSystem");
        return;
    }

    pState_->state = cgl::StateTypes::INITIALIZED;
}
