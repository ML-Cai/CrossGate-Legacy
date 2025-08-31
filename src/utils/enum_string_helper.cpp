// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <sstream>
#include "cgl/core/results.h"
#include "cgl/core/version.h"
#include "cgl/resources/resource_types.h"
#include "cgl/resources/anime_resource.h"
#include "cgl/utils/enum_string_helper.h"

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::Results& type) {
    switch (type) {
#define CGL_X(name) case cgl::Results::name: return #name;

        Results_ENUM_LIST

#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::CrossGateVersion& type) {
    switch (type) {
#define CGL_X(name) case cgl::CrossGateVersion::name: return #name;

        CrossGateVersion_ENUM_LIST

#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::GraphicsResourceSerialNumTypes& type) {
    switch (type) {
#define CGL_X(name) case cgl::GraphicsResourceSerialNumTypes::name: return #name;

        GraphicsResourceSerialNumTypes_ENUM_LIST

#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
const char* cgl::GetString(const cgl::EnvironmentPaletteTypes& type) {
    switch (type) {
#define CGL_X(name) case cgl::EnvironmentPaletteTypes::name: return #name;
        EnvironmentPaletteTypes_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::toStr(const cgl::GraphicsResourceSerialNum& sn) {
    std::ostringstream oss;
    oss << GetString(sn.type)
        << ":" << GetString(sn.version)
        << ":" << sn.value;
    return oss.str();
}

// -----------------------------------------------------------------------------
std::string cgl::toStr(const cgl::AnimeResourceSerialNum& sn) {
    std::ostringstream oss;
    oss << GetString(sn.version)
        << ":" << sn.value;
    return oss.str();
}