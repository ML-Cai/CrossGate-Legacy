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
    LOGI("WindowInitSystem::update");
    auto pWindowState = pECS->getSingleton<cgl::component::WindowState>();
    auto pCreateInfo = pECS->getSingleton<cgl::component::WindowCreateInfo>();
    assert(pWindowState != nullptr);
    assert(pCreateInfo != nullptr);

    // reset state
    pWindowState->state = cgl::StateTypes::INITIALIZING;

    // create window
    if (this->createWindow(pCreateInfo, pWindowState) == false) {
        return;
    }
    pWindowState->state = cgl::StateTypes::ACTIVE;

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
    std::stringstream ss;

    if (glfwInit() != GLFW_TRUE) {
        cgl::RaiseError(pWindowState, "Failed in glfwInit()");
        return false;
    }

    LOGD("Create window with size (" << pCreateInfo->width
         << "x" << pCreateInfo->height << "), title :" << pCreateInfo->title);

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
