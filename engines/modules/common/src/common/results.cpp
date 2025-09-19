// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/common/results.h"

// -----------------------------------------------------------------------------
const char* cgl::ToStr(const cgl::Results& type) {
    switch (type) {
#define CGL_X(name) case cgl::Results::name: return #name;
        CGL_RESULTS_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}
