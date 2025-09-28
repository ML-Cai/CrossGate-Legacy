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

#define CGL_ENGINE_STATE_TYPES_VALID_LIST   \
    CGL_X(RUNNING)                          \
    CGL_X(SHUTTING_DOWN)                    \
    CGL_X(FATAL_ERROR)                      \


#define CGL_ENGINE_STATE_TYPES_ENUM_FULL_LIST   \
    CGL_ENGINE_STATE_TYPES_VALID_LIST           \
    CGL_X(COUNT)                                \
    CGL_X(UNKNOWN)


enum class EngineStateTypes : uint8_t {
#define CGL_X(name) name,
    CGL_ENGINE_STATE_TYPES_ENUM_FULL_LIST
#undef CGL_X
};

}   // namespace cgl
