// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <assert.h>
#include <string_view>
#include "cgl/common/version.h"
#include "cgl/common/assets.h"
#include "assets_path_info.h"


cgl::CrossGateResourcePaths cgl::AcquireCrossGateResourcePath(
    cgl::CrossGateVersion version
) noexcept {
    assert(static_cast<size_t>(version) <
           static_cast<size_t>(CrossGateVersion::COUNT));

    // Static array of resource paths
    static constexpr cgl::CrossGateResourcePaths g_crossGateResourcePaths[] = {
        {
            cgl::CrossGateVersion::CG_VERSION_Classic,
            "GraphicInfo_66.bin",
            "Graphic_66.bin",
            "AnimeInfo_4.bin",
            "Anime_4.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK1,
            "GraphicInfoEx_5.bin",
            "GraphicEx_5.bin",
            "AnimeInfoEx_1.Bin",
            "AnimeEx_1.Bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK2_V1,
            "GraphicInfoV3_19.bin",
            "GraphicV3_19.bin",
            "AnimeInfoV3_8.bin",
            "AnimeV3_8.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK2_V2,
            "Puk2/GraphicInfo_PUK2_2.bin",
            "Puk2/Graphic_PUK2_2.bin",
            "Puk2/AnimeInfo_PUK2_4.bin",
            "Puk2/Anime_PUK2_4.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK3_V1,
            "Puk3/GraphicInfo_PUK3_1.bin",
            "Puk3/Graphic_PUK3_1.bin",
            "Puk3/AnimeInfo_PUK3_2.bin",
            "Puk3/Anime_PUK3_2.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V1,
            "GraphicInfo_Joy_125.bin",
            "Graphic_Joy_125.bin",
            "AnimeInfo_Joy_91.bin",
            "Anime_Joy_91.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V2,
            "GraphicInfo_Joy_CH1.bin",
            "Graphic_Joy_CH1.bin",
            "AnimeInfo_Joy_CH1.Bin",
            "Anime_Joy_CH1.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V3,
            "GraphicInfo_Joy_EX_9.bin",
            "Graphic_Joy_EX_9.bin",
            "AnimeInfo_Joy_EX_9.bin",
            "Anime_Joy_EX_9.bin",
        }
    };

    return g_crossGateResourcePaths[static_cast<size_t>(version)];
}


cgl::EnvironmentPaletteResourcePath cgl::AcquireEnvPalettePath(
    cgl::EnvironmentPaletteTypes paletteType
) noexcept {
    static constexpr cgl::EnvironmentPaletteResourcePath g_pPaths[] = {
        { cgl::EnvironmentPaletteTypes::Daytime,      "pal/palet_00.cgp" },
        { cgl::EnvironmentPaletteTypes::Evening,      "pal/palet_01.cgp" },
        { cgl::EnvironmentPaletteTypes::Night,        "pal/palet_02.cgp" },
        { cgl::EnvironmentPaletteTypes::EarlyMorning, "pal/palet_03.cgp" },

        // TODO(Miles) : decode the usage of other palette files in bin/pal/*

        // invalid path
        { cgl::EnvironmentPaletteTypes::Unknown,      "invalid-path" },
    };

    constexpr size_t pathCount = std::size(g_pPaths);
    size_t index = static_cast<size_t>(paletteType);

    if (index >= pathCount - 1) [[unlikely]] {
        index = pathCount - 1;
    }

    return g_pPaths[index];
}
