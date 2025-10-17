// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include <functional>
#include "window/window_components.h"

struct GLFWwindow;

// -----------------------------------------------------------------------------
namespace cgl {

class ECSCore;

// -----------------------------------------------------------------------------
class WindowInitSystem {
public:
    WindowInitSystem() = default;

    ~WindowInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool createWindow(const cgl::component::WindowCreateInfo* pCreateInfo,
                      cgl::component::WindowState*            pWindowState);
    GLFWwindow *window_;
};

// -----------------------------------------------------------------------------
class WindowInputSystem {
public:
    WindowInputSystem();

    ~WindowInputSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool sysInited_;
    void init(cgl::ECSCore* pECS);
    void updateEvents(cgl::ECSCore* pECS);

    using UpdateFunc = void (WindowInputSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    static void cursorPosCb(GLFWwindow* pWindow, double x, double y);
    static void mouseButtonCb(GLFWwindow* pWindow, int btn, int act, int);
    static void framebufferReSszeCb(GLFWwindow* window, int w, int h);

    cgl::component::CursorPositionState cursorPosState_[2];
    cgl::component::WindowState* pWindowState_;
    cgl::component::WindowHandle* pWindowHandle_;
};

}   // namespace cgl
