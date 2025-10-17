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
    LOGI("RenderDeviceDestroySystem::update");
    pECS->removeSingleton<cgl::component::PrimarySwapchain>();
    pECS->removeSingleton<cgl::component::PrimaryDeviceContext>();
}
