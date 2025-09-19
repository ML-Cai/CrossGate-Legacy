// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/common/version.h"

// -----------------------------------------------------------------------------
const char* cgl::ToStr(const cgl::CrossGateVersion& type) {
    switch (type) {
#define CGL_X(name) case cgl::CrossGateVersion::name: return #name;
        CGL_VERSION_TYPES_ENUM_FULL_LIST
#undef CGL_X
    default:
        return "UNKNOWN";
    }
}
