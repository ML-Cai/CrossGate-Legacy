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

// Define the core list of actual directions
#define CGL_DIRECTION_TYPES_VALID_LIST  \
    CGL_X(DIRECTION_NORTH)              \
    CGL_X(DIRECTION_NORTH_EAST)         \
    CGL_X(DIRECTION_EAST)               \
    CGL_X(DIRECTION_SOUTH_EAST)         \
    CGL_X(DIRECTION_SOUTH)              \
    CGL_X(DIRECTION_SOUTH_WEST)         \
    CGL_X(DIRECTION_WEST)               \
    CGL_X(DIRECTION_NORTH_WEST)


// Define the full list for the enum, including COUNT and special values
#define CGL_DIRECTION_TYPES_ENUM_FULL_LIST  \
    CGL_DIRECTION_TYPES_VALID_LIST          \
    CGL_X(COUNT)                            \
    CGL_X(UNKNOWN)


enum class DirectionTypes : uint8_t {
#define CGL_X(name) name,
    CGL_DIRECTION_TYPES_ENUM_FULL_LIST
#undef CGL_X
};

}   // namespace cgl
