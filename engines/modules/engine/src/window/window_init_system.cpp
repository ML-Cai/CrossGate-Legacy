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
#include "ecs.h"
#include "engine_components.h"
#include "window/window_components.h"
#include "window/window_init_system.h"


cgl::WindowInitSystem::WindowInitSystem() {
}

// -----------------------------------------------------------------------------
void cgl::WindowInitSystem::update(cgl::ECSCore* pECS) {
    LOGD("WindowInitSystem::update");
    auto pEngineState = pECS->getSingleton<cgl::component::EngineState>();
    auto pCreateInfo = pECS->getSingleton<cgl::component::WindowCreateInfo>();
    assert(pEngineState != nullptr);
    assert(pCreateInfo != nullptr);

    if (this->createWindow(pCreateInfo) == false) {
        pEngineState->state = cgl::EngineStateTypes::FATAL_ERROR;
        pEngineState->last_error_message = "Failed to create window";
    }

    // Add result handle to ecs
    pECS->addSingleton<cgl::component::WindowHandle>(window_, false);
}

// -----------------------------------------------------------------------------
bool cgl::WindowInitSystem::createWindow(
    const cgl::component::WindowCreateInfo* pCreateInfo
) {
    if (glfwInit() != GLFW_TRUE) {
        LOGE("Failed in glfwInit()");
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
        LOGE("Failed to create window with size (" << pCreateInfo->width
             << " x " << pCreateInfo->height
             << "), title :" << pCreateInfo->title);
        return false;
    }
    return true;
}
