// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#include "cgl/core/results.h"
#include "cgl/core/version.h"
#include "cgl/utils/enum_string_helper.h"

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::Results& type) {
    switch (type) {
#define Results_X(name) case cgl::Results::name: return #name;

        Results_ENUM_LIST

#undef Results_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::CrossGateVersion& type) {
    switch (type) {
#define CrossGateVersion_X(name) case cgl::CrossGateVersion::name: return #name;

        CrossGateVersion_ENUM_LIST

#undef CrossGateVersion_X
    default:
        return "Unknown";
    }
}
