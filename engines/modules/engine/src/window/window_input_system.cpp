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
    : cursorPosState_{},
      pWindowState_(nullptr),
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
    glfwSetCursorPosCallback(pWindow, cursorPosCb);
    glfwSetMouseButtonCallback(pWindow, mouseButtonCb);
    glfwSetFramebufferSizeCallback(pWindow, framebufferReSszeCb);

    // switch to next stage
    updater_ = &WindowInputSystem::updateEvents;
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

    // check cursor pos updated or not
    float x = cursorPosState_[0].x;
    float y = cursorPosState_[0].y;
    float dx = x - cursorPosState_[1].x;
    float dy = y - cursorPosState_[1].y;
    if (dx != 0.0f || dy != 0.0f) {
        cursorPosState_[1].x = x;
        cursorPosState_[1].y = y;
    }
}

// -----------------------------------------------------------------------------
void cgl::WindowInputSystem::cursorPosCb(
    GLFWwindow* pWindow,
    double      xpos,
    double      ypos
) {
    auto userPtr = glfwGetWindowUserPointer(pWindow);
    auto thiz = static_cast<cgl::WindowInputSystem*>(userPtr);

    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);
    thiz->cursorPosState_[0].x = x;
    thiz->cursorPosState_[0].y = y;
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
