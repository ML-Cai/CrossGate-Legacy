// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "render/render_components.h"
#include "render/render_systems.h"

// -----------------------------------------------------------------------------
void cgl::RenderDeviceDestroySystem::update(cgl::ECSCore* pECS) {
    LOGI("Destroy render device.");
    auto pState     = pECS->getSingleton<cgl::component::RenderDeviceState>();
    assert(pState != nullptr);

    // release objects
    pState->pSwapchain = nullptr;
    pState->pDevice = nullptr;

    // remove singleton
    pECS->removeSingleton<cgl::component::RenderDeviceState>();
}
