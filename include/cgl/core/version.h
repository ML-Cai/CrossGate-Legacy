// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <stdint.h>
#include <string_view>

namespace cgl {

// -----------------------------------------------------------------------------
// Version related section
// -----------------------------------------------------------------------------

#define CrossGateVersion_ENUM_LIST \
    CGL_X(CG_VERSION_Classic)  /* GraphicInfo              */ \
    CGL_X(CG_VERSION_PUK1)     /* GraphicInfoEx_*          */ \
    CGL_X(CG_VERSION_PUK2_V1)  /* GraphicInfoV3_*          */ \
    CGL_X(CG_VERSION_PUK2_V2)  /* Puk2/GraphicInfo_PUK2_*  */ \
    CGL_X(CG_VERSION_PUK3_V1)  /* Puk3/GraphicInfo_PUK3_*  */ \
    CGL_X(CG_VERSION_PUK4_V1)  /* GraphicInfo_Joy_*        */ \
    CGL_X(CG_VERSION_PUK4_V2)  /* GraphicInfo_Joy_CH*      */ \
    CGL_X(CG_VERSION_PUK4_V3)  /* GraphicInfo_Joy_EX_*     */ \
    CGL_X(Count)                                              \
    CGL_X(CG_VERSION_UNKNOWN)

enum class CrossGateVersion : uint8_t {
#define CGL_X(name) name,
    CrossGateVersion_ENUM_LIST
#undef CGL_X
};

struct CrossGateResourcePaths {
    cgl::CrossGateVersion version;
    std::string_view      graphicsInfoSubPath;
    std::string_view      graphicsDataSubPath;
    std::string_view      animeIndexSubPath;
    std::string_view      animeDataSubPath;
};
}   // namespace cgl
