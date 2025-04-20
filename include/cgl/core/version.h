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
    CrossGateVersion_X(CG_VERSION_Classic)  /* GraphicInfo              */ \
    CrossGateVersion_X(CG_VERSION_PUK1)     /* GraphicInfoEx_*          */ \
    CrossGateVersion_X(CG_VERSION_PUK2_V1)  /* GraphicInfoV3_*          */ \
    CrossGateVersion_X(CG_VERSION_PUK2_V2)  /* Puk2/GraphicInfo_PUK2_*  */ \
    CrossGateVersion_X(CG_VERSION_PUK3_V1)  /* Puk3/GraphicInfo_PUK3_*  */ \
    CrossGateVersion_X(CG_VERSION_PUK4_V1)  /* GraphicInfo_Joy_*        */ \
    CrossGateVersion_X(CG_VERSION_PUK4_V2)  /* GraphicInfo_Joy_CH*      */ \
    CrossGateVersion_X(CG_VERSION_PUK4_V3)  /* GraphicInfo_Joy_EX_*     */ \
    CrossGateVersion_X(Count)                                              \
    CrossGateVersion_X(CG_VERSION_UNKNOWN)

enum class CrossGateVersion : uint8_t {
#define CrossGateVersion_X(name) name,
    CrossGateVersion_ENUM_LIST
#undef CrossGateVersion_X
};

struct CrossGateResourcePaths {
    cgl::CrossGateVersion version;
    std::string_view      graphicsInfoSubPath;
    std::string_view      graphicsDataSubPath;
    std::string_view      animeIndexSubPath;
    std::string_view      animeDataSubPath;
};
}   // namespace cgl
