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

// -----------------------------------------------------------------------------
// forward declaration
struct GLFWwindow;

namespace cgl {

class ECSCore;

}   // namespace cgl

// -----------------------------------------------------------------------------
namespace cgl {

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
    cgl::component::CursorPositionState cursorPosState_[2];
};

}   // namespace cgl
