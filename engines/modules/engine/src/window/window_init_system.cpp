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
#include <sstream>
#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "window/window_components.h"
#include "window/window_systems.h"

// -----------------------------------------------------------------------------
void cgl::WindowInitSystem::update(cgl::ECSCore* pECS) {
    auto pWindowState = pECS->getSingleton<cgl::component::WindowState>();
    auto pCreateInfo = pECS->getSingleton<cgl::component::WindowCreateInfo>();
    assert(pWindowState != nullptr);
    assert(pCreateInfo != nullptr);

    // check state
    if ((pWindowState->state != cgl::StateTypes::UNKNOWN) &&
        (pWindowState->state != cgl::StateTypes::INITIALIZING)) {
        cgl::RaiseError(pWindowState, "The `WindowInitSystem` updates data and "
            "systems only when the `WindowState` is in the `UNKNOWN` or "
            "`INITIALIZING` state. Please verify the flow.");
        return;
    }

    // create window
    if (this->createWindow(pCreateInfo, pWindowState) == false) {
        return;
    }

    // Add result handle to ecs
    pECS->addSingleton<cgl::component::WindowHandle>(
            window_,
            false,
            pCreateInfo->width,
            pCreateInfo->height);
}

// -----------------------------------------------------------------------------
bool cgl::WindowInitSystem::createWindow(
    const cgl::component::WindowCreateInfo* pCreateInfo,
    cgl::component::WindowState*            pWindowState
) {
    if (glfwInit() != GLFW_TRUE) {
        cgl::RaiseError(pWindowState, "Failed in glfwInit()");
        return false;
    }

    LOGD("Create window with size "
         << "(" << pCreateInfo->width  << "x" << pCreateInfo->height << "), "
         << "title :" << pCreateInfo->title);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(pCreateInfo->width,
                               pCreateInfo->height,
                               pCreateInfo->title.c_str(),
                               nullptr,
                               nullptr);
    if (window_ == nullptr) {
        cgl::RaiseError(pWindowState,
            "Failed to create window with size (", pCreateInfo->width,
            " x ", pCreateInfo->height, "), title :", pCreateInfo->title);
        return false;
    }
    return true;
}
