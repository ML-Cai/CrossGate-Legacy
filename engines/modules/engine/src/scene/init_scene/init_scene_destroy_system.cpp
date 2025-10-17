// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "scene/init_scene/init_scene_systems.h"
#include "render/render_components.h"
#include "engine/ecs.h"

// -----------------------------------------------------------------------------
void cgl::InitSceneDestroySystem::update(cgl::ECSCore* pECS) {
    LOGD("InitSceneDestroySystem::update");

    auto pState = pECS->getSingleton<cgl::component::SceneState>();
    pState->state = cgl::StateTypes::SHUTTING_DOWN;

    auto pDevCtx = pECS->getSingleton<cgl::component::PrimaryDeviceContext>();
    if (pDevCtx->pDevice != nullptr) {
        pDevCtx->pDevice->waitIdle();
        pECS->removeSingleton<cgl::component::PrimarySceneRenderFrame>();
        pECS->removeSingleton<cgl::component::PrimaryRenderSyncObjects>();
    }


    pState->state = cgl::StateTypes::SHUTDOWN;
}
