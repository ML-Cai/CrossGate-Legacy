// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <sstream>
#include "cgl/common/results.h"
#include "cgl/common/version.h"
#include "cgl/common/assets.h"

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::Results& type) {
    switch (type) {
#define CGL_X(name) case cgl::Results::name: return #name;
        CGL_RESULTS_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::CrossGateVersion& type) {
    switch (type) {
#define CGL_X(name) case cgl::CrossGateVersion::name: return #name;
        CGL_VERSION_TYPES_ENUM_FULL_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::GraphicsResourceSerialNumTypes& type) {
    switch (type) {
#define CGL_X(name) case cgl::GraphicsResourceSerialNumTypes::name: return #name;
        GRAPHICS_ASSETS_SERIAL_NUM_TYPE_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::MotionTypes& type) {
    switch (type) {
#define CGL_X(name) case cgl::MotionTypes::name: return #name;
        CGL_MOTION_TYPES_ENUM_FULL_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::EnvironmentPaletteTypes& type) {
    switch (type) {
#define CGL_X(name) case cgl::EnvironmentPaletteTypes::name: return #name;
        EnvironmentPaletteTypes_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}

// -----------------------------------------------------------------------------
std::string cgl::ToStr(const cgl::GraphicsResourceSerialNum& sn) {
    std::ostringstream oss;
    oss << cgl::ToStr(sn.type)
        << ":" << cgl::ToStr(sn.version)
        << ":" << sn.value;
    return oss.str();
}

// -----------------------------------------------------------------------------
std::string  cgl::ToStr(const cgl::AnimeResourceSerialNum& sn) {
    std::ostringstream oss;
    oss << cgl::ToStr(sn.version)
        << ":" << sn.value;
    return oss.str();
}