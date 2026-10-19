// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "render/frame_render_systems.h"
#include "render/render_components.h"

// -----------------------------------------------------------------------------
void cgl::FrameRenderDestroySystem::update(cgl::ECSCore* pECS) {
    LOGD("Destroy engine frame resources");

    auto pState = pECS->getSingleton<cgl::component::FrameRenderState>();
    assert(pState != nullptr);

    if (pState->state == cgl::StateTypes::RELEASE) return;

    auto pDevState= pECS->getSingleton<cgl::component::RenderDeviceState>();
    if (pDevState->pDevice != nullptr) {
        pDevState->pDevice->waitIdle();
        pECS->removeSingleton<cgl::component::PrimarySceneRenderFrame>();
        pECS->removeSingleton<cgl::component::PrimaryRenderSyncObjects>();
    }
    pState->state = cgl::StateTypes::RELEASE;
}
