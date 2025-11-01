// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <assert.h>
#include <string_view>
#include "cgl/common/version.h"

namespace cgl {

struct CrossGateResourcePaths {
    cgl::CrossGateVersion version;
    std::string_view      graphicsInfoSubPath;
    std::string_view      graphicsDataSubPath;
    std::string_view      animeIndexSubPath;
    std::string_view      animeDataSubPath;
};

cgl::CrossGateResourcePaths AcquireCrossGateResourcePath(
    cgl::CrossGateVersion version
) noexcept;


struct EnvironmentPaletteResourcePath {
    cgl::EnvironmentPaletteTypes paletteType;
    std::string_view             paletteFilePath;
};

cgl::EnvironmentPaletteResourcePath AcquireEnvPalettePath(
    cgl::EnvironmentPaletteTypes paletteType
) noexcept;

}   // namespace cgl
