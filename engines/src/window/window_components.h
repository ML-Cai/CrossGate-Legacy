// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include "cgl/common/input.h"
#include "cgl/common/states.h"

namespace cgl {
namespace component {

// -----------------------------------------------------------------------------
struct WindowCreateInfo {
    int width;
    int height;
    std::string title;
};

struct KeyboardInputState {
    bool restartSceneRequested = false;
    bool restartKeyDown = false;
    bool newBatchRequested = false, newBatchKeyDown = false;
    float panX = 0, panY = 0;
    float moveX = 0, moveY = 0;
};

// -----------------------------------------------------------------------------
struct WindowHandle {
    void* nativeHandle;     // handle of GLFWwindow
    bool windowShouldClose;
    uint32_t width;
    uint32_t height;
    std::string pendingTitle; // Consumed by the platform input update.
};

// -----------------------------------------------------------------------------
struct CursorPositionState {
    double x = 0, y = 0;
    uint32_t windowWidth = 0, windowHeight = 0; // Logical GLFW client units.
    bool inside = false; // Hovered and focused; false on leave/minimize/focus loss.
};

// -----------------------------------------------------------------------------
struct MouseInputState {
    cgl::MouseButtonTypes button;
    cgl::InputActionTypes inputAction;
};

// -----------------------------------------------------------------------------
struct WindowState {
    cgl::StateTypes state;
    std::string lastError;
};


}   // namespace component
}   // namespace cgl
