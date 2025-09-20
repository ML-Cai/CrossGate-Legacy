// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <stdint.h>

namespace cgl {

// -----------------------------------------------------------------------------
// Version related section
// -----------------------------------------------------------------------------
#define CGL_VERSION_TYPES_VALID_LIST                            \
    CGL_X(CG_VERSION_Classic)  /* GraphicInfo              */   \
    CGL_X(CG_VERSION_PUK1)     /* GraphicInfoEx_*          */   \
    CGL_X(CG_VERSION_PUK2_V1)  /* GraphicInfoV3_*          */   \
    CGL_X(CG_VERSION_PUK2_V2)  /* Puk2/GraphicInfo_PUK2_*  */   \
    CGL_X(CG_VERSION_PUK3_V1)  /* Puk3/GraphicInfo_PUK3_*  */   \
    CGL_X(CG_VERSION_PUK4_V1)  /* GraphicInfo_Joy_*        */   \
    CGL_X(CG_VERSION_PUK4_V2)  /* GraphicInfo_Joy_CH*      */   \
    CGL_X(CG_VERSION_PUK4_V3)  /* GraphicInfo_Joy_EX_*     */   \

#define CGL_VERSION_TYPES_ENUM_FULL_LIST    \
    CGL_VERSION_TYPES_VALID_LIST            \
    CGL_X(COUNT)                            \
    CGL_X(UNKNOWN)


enum class CrossGateVersion : uint8_t {
#define CGL_X(name) name,
    CGL_VERSION_TYPES_ENUM_FULL_LIST
#undef CGL_X
};

std::string ToStr(const cgl::CrossGateVersion& type);

}   // namespace cgl
