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

// -----------------------------------------------------------------------------
#define CGL_STATE_TYPES_VALID_LIST    \
    CGL_X(UNINITIALIZED)                    \
    CGL_X(INITIALIZING)                     \
    CGL_X(INITIALIZED)                      \
    CGL_X(RUNNING)                          \
    CGL_X(ACTIVE)                           \
    CGL_X(UNLOADING)                        \
    CGL_X(SHUTTING_DOWN)                    \
    CGL_X(SHUTDOWN)                         \
    CGL_X(RELEASE)                          \
    CGL_X(ERROR)                            \

#define CGL_STATE_TYPES_ENUM_FULL_LIST    \
    CGL_STATE_TYPES_VALID_LIST            \
    CGL_X(COUNT)                                \
    CGL_X(UNKNOWN)

enum class StateTypes : uint8_t {
#define CGL_X(name) name,
    CGL_STATE_TYPES_ENUM_FULL_LIST
#undef CGL_X
};


}   // namespace cgl
