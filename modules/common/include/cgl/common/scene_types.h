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
// Scene
// -----------------------------------------------------------------------------
#define CGL_SCENE_TYPES_ENUM_LIST   \
    CGL_X(NullScene)                \
    CGL_X(InitScene)                \
    CGL_X(LoginScene)               \
    CGL_X(ServerSelectionScene)     \
    CGL_X(CharacterSelectionScene)  \
    CGL_X(MainScene)

#define CGL_SCENE_TYPES_ENUM_FULL_LIST  \
    CGL_SCENE_TYPES_ENUM_LIST           \
    CGL_X(Count)                        \
    CGL_X(Unknown)

enum class SceneTypes : uint32_t {
#define CGL_X(name) name,
    CGL_SCENE_TYPES_ENUM_FULL_LIST
#undef CGL_X
};

}   // namespace cgl
