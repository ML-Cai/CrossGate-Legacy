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

// -----------------------------------------------------------------------------
struct WindowHandle {
    void* nativeHandle;     // handle of GLFWwindow
    bool windowShouldClose;
    uint32_t width;
    uint32_t height;
};

// -----------------------------------------------------------------------------
struct CursorPositionState {
    float x;
    float y;
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
