// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "scene/init_scene/init_scene_systems.h"
#include "render/render_components.h"

// -----------------------------------------------------------------------------
void cgl::InitSceneRenderDestroySystem::update(cgl::ECSCore* pECS) {
    LOGD("Destroy all render objects in InitScene");

    auto pState = pECS->getSingleton<cgl::component::SceneRenderState>();
    assert(pState != nullptr);

    if (pState->state == cgl::StateTypes::RELEASE) {
        cgl::RaiseError(pState, "The `InitSceneRenderDestroySystem` updates "
            "data and systems only when the `RenderState` is NOT in the "
            "`RELEASE` state. Please verify the flow.");
        return;
    }

    auto pDevState= pECS->getSingleton<cgl::component::RenderDeviceState>();
    if (pDevState->pDevice != nullptr) {
        pDevState->pDevice->waitIdle();
        pECS->removeSingleton<cgl::component::PrimarySceneRenderFrame>();
        pECS->removeSingleton<cgl::component::PrimaryRenderSyncObjects>();
    }
}
