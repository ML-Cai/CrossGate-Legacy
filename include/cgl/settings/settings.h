// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string_view>
#include "cgl/core/version.h"

namespace cgl {

enum class EnvironmentPaletteTypes : uint8_t;

struct EnvironmentPaletteResourcePath;

struct Settings {
    /**
     * @brief This value will pointer to the resource directory of Cross Gate
     * game, the struct of folder will be :
     *   crossGateResourceRootDir
     *     |- bin
     *     |- map
     *     | ....
     */
    std::string_view crossGateResourceRootDir;

    const cgl::CrossGateResourcePaths& crossGateResourcePath(
        cgl::CrossGateVersion version) const noexcept;

    const cgl::EnvironmentPaletteResourcePath& envPalettePath(
        cgl::EnvironmentPaletteTypes paletteType) const noexcept;
};

}   // namespace cgl
