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

using cgl::component::PrimaryDeviceContext;
using cgl::component::PrimarySwapchain;

// -----------------------------------------------------------------------------
bool cgl::RenderDeviceInitSystem::initEssentialRenderObjects(
    cgl::ECSCore* pECS
) {
    auto pWinHandle = pECS->getSingleton<cgl::component::WindowHandle>();
    assert(pWinHandle != nullptr);

    // create render device
    auto pDevice = cgl::IDevice::create(pWinHandle->nativeHandle, true);
    if (pDevice == nullptr) {
        return false;
    }

    // create swapchain
    auto pSwapchain = cgl::ISwapchain::create(
                        pDevice.get(),
                        pWinHandle->nativeHandle);
    if (pSwapchain == nullptr) {
        return false;
    }

    // Add result handle to ecs
    pECS->addSingleton<PrimaryDeviceContext>(std::move(pDevice));
    pECS->addSingleton<PrimarySwapchain>(std::move(pSwapchain));

    return true;
}

// -----------------------------------------------------------------------------
void cgl::RenderDeviceInitSystem::update(cgl::ECSCore* pECS) {
    LOGI("RenderDeviceInitSystem::update");
    auto pState = pECS->getSingleton<cgl::component::RenderState>();
    assert(pState != nullptr);

    if (initEssentialRenderObjects(pECS) == false) {
        cgl::RaiseError(pState, "Failed to create essential render objects");
        return;
    }
}
