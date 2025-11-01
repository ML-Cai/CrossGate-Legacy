// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <cstdint>

namespace cgl {

#define CGL_MOUSE_BUTTON_TYPES_ENUM_LIST \
    CGL_X(Left)     \
    CGL_X(Right)    \
    CGL_X(Middle)

#define CGL_MOUSE_BUTTON_TYPES_ENUM_LIST_FULL_LIST   \
    CGL_MOUSE_BUTTON_TYPES_ENUM_LIST                 \
    CGL_X(Count)                                     \
    CGL_X(Unknown)

enum class MouseButtonTypes : uint8_t {
#define CGL_X(name) name,
    CGL_MOUSE_BUTTON_TYPES_ENUM_LIST_FULL_LIST
#undef CGL_X
};


#define CGL_INPUT_ACTION_TYPES_ENUM_LIST  \
    CGL_X(Press)       \
    CGL_X(Release)     \
    CGL_X(Repeat)

#define CGL_INPUT_ACTION_TYPES_ENUM_FULL_LIST   \
    CGL_INPUT_ACTION_TYPES_ENUM_LIST            \
    CGL_X(Count)                                \
    CGL_X(Unknown)

enum class InputActionTypes : uint8_t {
#define CGL_X(name) name,
    CGL_INPUT_ACTION_TYPES_ENUM_FULL_LIST
#undef CGL_X

};

}   // namespace cgl
