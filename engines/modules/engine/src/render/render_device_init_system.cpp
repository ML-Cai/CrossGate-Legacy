// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <assert.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "cgl/trace/logger.h"
#include "cgl/render/device.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/engine_components.h"
#include "window/window_components.h"
#include "render/render_components.h"
#include "render/render_systems.h"

// -----------------------------------------------------------------------------
void cgl::RenderDeviceInitSystem::update(cgl::ECSCore* pECS) {
    LOGI("RenderDeviceInitSystem::update");
    auto pState     = pECS->getSingleton<cgl::component::RenderDeviceState>();
    auto pWinHandle = pECS->getSingleton<cgl::component::WindowHandle>();
    assert(pState != nullptr);
    assert(pWinHandle != nullptr);

    // check state
    if ((pState->state != cgl::StateTypes::UNKNOWN) &&
        (pState->state != cgl::StateTypes::INITIALIZING)) {
        cgl::RaiseError(pState, "The `RenderDeviceInitSystem` updates data and "
            "systems only when the `RenderState` is in the `UNKNOWN` or "
            "`INITIALIZING`state. Please verify the flow.");
        return;
    }

    // create render device
    auto pDevice = cgl::IDevice::create(pWinHandle->nativeHandle, true);
    if (pDevice == nullptr) {
        cgl::RaiseError(pState, "Failed to primary render device.");
        return;
    }

    // create swapchain
    auto pSwapchain = cgl::ISwapchain::create(pDevice.get(),
                                              pWinHandle->nativeHandle);
    if (pSwapchain == nullptr) {
        cgl::RaiseError(pState, "Failed to primary swapchain.");
        return;
    }

    // Add result handle to ecs
    pState->pDevice    = std::move(pDevice);
    pState->pSwapchain = std::move(pSwapchain);
}
