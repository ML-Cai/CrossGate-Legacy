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
#define CGL_MOTION_TYPES_VALID_LIST  \
    CGL_X(STANDARD)    \
    CGL_X(WALK)        \
    CGL_X(PRE_ATTACH)  \
    CGL_X(RUN)         \
    CGL_X(RESERVED_1)  \
    CGL_X(ATTACK)      \
    CGL_X(CAST_SPELL)  \
    CGL_X(THROWING)    \
    CGL_X(INJURED)     \
    CGL_X(DEFENSE)     \
    CGL_X(DEAD)        \
    CGL_X(SIT)         \
    CGL_X(HELLO)       \
    CGL_X(HAPPY)       \
    CGL_X(ANGRY)       \
    CGL_X(SAD)         \
    CGL_X(NOD)         \
    CGL_X(ROCK)        \
    CGL_X(SCISSORS)    \
    CGL_X(PAPER)       \
    CGL_X(FISHING)


// Define the full list for the enum, including COUNT and special values
#define CGL_MOTION_TYPES_ENUM_FULL_LIST  \
    CGL_MOTION_TYPES_VALID_LIST          \
    CGL_X(COUNT)                         \
    CGL_X(UNKNOWN)


enum class MotionTypes : uint8_t {
#define CGL_X(name) name,
    CGL_MOTION_TYPES_ENUM_FULL_LIST
#undef CGL_X
};

}   // namespace cgl
