// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

namespace cgl {

// -----------------------------------------------------------------------------
// Results
// -----------------------------------------------------------------------------
#define CGL_RESULTS_ENUM_LIST      \
    CGL_X(Success)                 \
    CGL_X(Fail)                    \
    CGL_X(OutOfMemory)             \
    CGL_X(InvalidFile)             \
    CGL_X(InvalidArgs)             \
    CGL_X(IndexNotExist)           \
    CGL_X(Unknown)

enum class Results : unsigned int {
#define CGL_X(name) name,
    CGL_RESULTS_ENUM_LIST
#undef CGL_X
};

const char* ToStr(const cgl::Results& type);

}   // namespace cgl
