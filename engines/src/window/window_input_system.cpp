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
#include "cgl/common/input.h"
#include "cgl/common/formatters.h"
#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/engine_components.h"
#include "window/window_components.h"
#include "window/window_systems.h"


// -----------------------------------------------------------------------------
namespace {

cgl::MouseButtonTypes fromGLFWMouseButton(int32_t glfw_mouse_button_value) {
    switch (glfw_mouse_button_value) {
    case GLFW_MOUSE_BUTTON_LEFT:
        return cgl::MouseButtonTypes::Left;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return cgl::MouseButtonTypes::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return cgl::MouseButtonTypes::Middle;
    default:
        return cgl::MouseButtonTypes::Unknown;
    }
}

cgl::InputActionTypes fromGLFWInputAction(int32_t glfw_action_value) {
    switch (glfw_action_value) {
    case GLFW_PRESS:
        return cgl::InputActionTypes::Press;
    case GLFW_RELEASE:
        return cgl::InputActionTypes::Release;
    case GLFW_REPEAT:
        return cgl::InputActionTypes::Repeat;
    default:
        return cgl::InputActionTypes::Unknown;
    }
}

}   // namespace

// -----------------------------------------------------------------------------
cgl::WindowInputSystem::WindowInputSystem()
    : pWindowState_(nullptr),
      pWindowHandle_(nullptr) {
    updater_ = &WindowInputSystem::init;
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::init(cgl::ECSCore* pECS) {
    pWindowState_ = pECS->getSingleton<cgl::component::WindowState>();
    pWindowHandle_ = pECS->getSingleton<cgl::component::WindowHandle>();
    assert(pWindowState_ != nullptr);
    assert(pWindowHandle_ != nullptr);

    // check state
    if (pWindowState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pWindowState_, "The `WindowInputSystem` updates data "
            "and systems only when the `WindowState` is in the `RUNNING` "
            "state. Please verify the flow.");
        return;
    }

    auto pWindow = static_cast<GLFWwindow *>(pWindowHandle_->nativeHandle);
    glfwSetWindowUserPointer(pWindow, this);
    glfwSetMouseButtonCallback(pWindow, mouseButtonCb);
    glfwSetFramebufferSizeCallback(pWindow, framebufferReSszeCb);

    // switch to next stage
    updater_ = &WindowInputSystem::updateEvents;
    updateEvents(pECS);
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::updateEvents(cgl::ECSCore* pECS) {
    assert(pWindowState_ != nullptr);
    assert(pWindowHandle_ != nullptr);

    // check state
    if (pWindowState_->state != cgl::StateTypes::RUNNING) {
        cgl::RaiseError(pWindowState_, "The `WindowInputSystem` updates data "
            "and systems only when the `WindowState` is in the `RUNNING` "
            "state. Please verify the flow.");
        return;
    }

    auto pWindow = static_cast<GLFWwindow *>(pWindowHandle_->nativeHandle);
    if (glfwWindowShouldClose(pWindow) > 0) {
        LOGD("Window closed");
        pWindowHandle_->windowShouldClose = true;
        return;
    }

    glfwPollEvents();

    if(!pWindowHandle_->pendingTitle.empty()) {
        glfwSetWindowTitle(pWindow,pWindowHandle_->pendingTitle.c_str());
        pWindowHandle_->pendingTitle.clear();
    }
    auto* keyboard = pECS->getSingleton<cgl::component::KeyboardInputState>();
    const bool down = glfwGetWindowAttrib(pWindow, GLFW_FOCUSED) && glfwGetKey(pWindow, GLFW_KEY_R) == GLFW_PRESS;
    if (down && !keyboard->restartKeyDown) keyboard->restartSceneRequested = true;
    keyboard->restartKeyDown = down;
    const bool batchDown=glfwGetWindowAttrib(pWindow, GLFW_FOCUSED) && glfwGetKey(pWindow, GLFW_KEY_N)==GLFW_PRESS;
    if(batchDown && !keyboard->newBatchKeyDown) keyboard->newBatchRequested=true;
    keyboard->newBatchKeyDown=batchDown;
    keyboard->panX = keyboard->panY = 0;
    keyboard->moveX = keyboard->moveY = 0;
    if (glfwGetWindowAttrib(pWindow, GLFW_FOCUSED)) {
        keyboard->moveX = float(glfwGetKey(pWindow, GLFW_KEY_D) == GLFW_PRESS) -
            float(glfwGetKey(pWindow, GLFW_KEY_A) == GLFW_PRESS);
        keyboard->moveY = float(glfwGetKey(pWindow, GLFW_KEY_S) == GLFW_PRESS) -
            float(glfwGetKey(pWindow, GLFW_KEY_W) == GLFW_PRESS);
        keyboard->panX = float(glfwGetKey(pWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) -
            float(glfwGetKey(pWindow, GLFW_KEY_LEFT) == GLFW_PRESS);
        keyboard->panY = float(glfwGetKey(pWindow, GLFW_KEY_DOWN) == GLFW_PRESS) -
            float(glfwGetKey(pWindow, GLFW_KEY_UP) == GLFW_PRESS);
    }

    auto* cursor = pECS->getSingleton<cgl::component::CursorPositionState>();
    int width = 0, height = 0;
    glfwGetWindowSize(pWindow, &width, &height);
    glfwGetCursorPos(pWindow, &cursor->x, &cursor->y);
    cursor->windowWidth = width > 0 ? uint32_t(width) : 0;
    cursor->windowHeight = height > 0 ? uint32_t(height) : 0;
    cursor->inside = width > 0 && height > 0 && !glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED) &&
        glfwGetWindowAttrib(pWindow, GLFW_HOVERED) && glfwGetWindowAttrib(pWindow, GLFW_FOCUSED) &&
        cursor->x >= 0 && cursor->y >= 0 && cursor->x < width && cursor->y < height;
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::mouseButtonCb(
    GLFWwindow* pWindow,
    int         button,
    int         action,
    int         mods
) {
    auto userPtr = glfwGetWindowUserPointer(pWindow);
    auto thiz = static_cast<cgl::WindowInputSystem*>(userPtr);

    cgl::component::MouseInputState inputState {
        .button      = fromGLFWMouseButton(button),
        .inputAction = fromGLFWInputAction(action)
    };
    LOGD("MouseInput " << inputState.button << " , " << inputState.inputAction);
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::framebufferReSszeCb(
    GLFWwindow* window,
    int w,
    int h
) {
    LOGW("Window size " << w << " , " << h);
}
