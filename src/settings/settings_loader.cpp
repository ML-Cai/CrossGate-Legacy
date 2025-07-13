// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <assert.h>
#include <filesystem>
#include "cgl/settings/settings.h"
#include "cgl/settings/settings_loader.h"
#include "cgl/resources/resource_types.h"

using cgl::SettingsLoader;

// -----------------------------------------------------------------------------
// cgl::Settings
// -----------------------------------------------------------------------------
const cgl::CrossGateResourcePaths& cgl::Settings::crossGateResourcePath(
    cgl::CrossGateVersion version
) const noexcept {
    assert(static_cast<size_t>(version) <
           static_cast<size_t>(CrossGateVersion::Count));

    // Static array of resource paths
    static constexpr cgl::CrossGateResourcePaths g_crossGateResourcePaths[] = {
        {
            cgl::CrossGateVersion::CG_VERSION_Classic,
            "bin/GraphicInfo_66.bin",
            "bin/Graphic_66.bin",
            "bin/AnimeInfo_4.bin",
            "bin/Anime_4.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK1,
            "bin/GraphicInfoEx_5.bin",
            "bin/GraphicEx_5.bin",
            "bin/AnimeInfoEx_1.Bin",
            "bin/AnimeEx_1.Bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK2_V1,
            "bin/GraphicInfoV3_19.bin",
            "bin/GraphicV3_19.bin",
            "bin/AnimeInfoV3_8.bin",
            "bin/AnimeV3_8.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK2_V2,
            "bin/Puk2/GraphicInfo_PUK2_2.bin",
            "bin/Puk2/Graphic_PUK2_2.bin",
            "bin/Puk2/AnimeInfo_PUK2_4.bin",
            "bin/Puk2/Anime_PUK2_4.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK3_V1,
            "bin/Puk3/GraphicInfo_PUK3_1.bin",
            "bin/Puk3/Graphic_PUK3_1.bin",
            "bin/Puk3/AnimeInfo_PUK3_2.bin",
            "bin/Puk3/Anime_PUK3_2.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V1,
            "bin/GraphicInfo_Joy_125.bin",
            "bin/Graphic_Joy_125.bin",
            "bin/AnimeInfo_Joy_91.bin",
            "bin/Anime_Joy_91.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V2,
            "bin/GraphicInfo_Joy_CH1.bin",
            "bin/Graphic_Joy_CH1.bin",
            "bin/AnimeInfo_Joy_CH1.Bin",
            "bin/Anime_Joy_CH1.bin",
        },
        {
            cgl::CrossGateVersion::CG_VERSION_PUK4_V3,
            "bin/GraphicInfo_Joy_EX_9.bin",
            "bin/Graphic_Joy_EX_9.bin",
            "bin/AnimeInfo_Joy_EX_9.bin",
            "bin/Anime_Joy_EX_9.bin",
        }
    };

    return g_crossGateResourcePaths[static_cast<size_t>(version)];
}

const cgl::EnvironmentPaletteResourcePath& cgl::Settings::envPalettePath(
    cgl::EnvironmentPaletteTypes paletteType
) const noexcept {
    static constexpr cgl::EnvironmentPaletteResourcePath g_pPaths[] = {
        { cgl::EnvironmentPaletteTypes::Daytime,      "bin/pal/palet_00.cgp" },
        { cgl::EnvironmentPaletteTypes::Evening,      "bin/pal/palet_01.cgp" },
        { cgl::EnvironmentPaletteTypes::Night,        "bin/pal/palet_02.cgp" },
        { cgl::EnvironmentPaletteTypes::EarlyMorning, "bin/pal/palet_03.cgp" },

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

// -----------------------------------------------------------------------------
// cgl::SettingsLoader::Impl
// -----------------------------------------------------------------------------
class cgl::SettingsLoader::Impl {
 public:
    Impl();

    ~Impl();

    cgl::Results load() noexcept;

    const cgl::Settings* settings() const noexcept;

 private:
    bool loaded_;
    cgl::Settings setting_;
};

// -----------------------------------------------------------------------------
SettingsLoader::Impl::Impl()
    : loaded_(false) {}

// -----------------------------------------------------------------------------
SettingsLoader::Impl::~Impl() {
}

// -----------------------------------------------------------------------------
cgl::Results SettingsLoader::Impl::load() noexcept {
    // load config via direct assign the value in this moment.
    setting_.crossGateResourceRootDir = "C:\\Users\\Dana\\Downloads\\cg_data\\";

    loaded_ = true;
    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
const cgl::Settings* SettingsLoader::Impl::settings() const noexcept {
    if (loaded_ == false) {
        return nullptr;
    }
    return &setting_;
}

// -----------------------------------------------------------------------------
// cgl::SettingsLoader's Impl
// -----------------------------------------------------------------------------
SettingsLoader::SettingsLoader() {
    pImpl_ = std::make_unique<cgl::SettingsLoader::Impl>();
}

// -----------------------------------------------------------------------------
SettingsLoader::~SettingsLoader() {
}

// -----------------------------------------------------------------------------
cgl::Results SettingsLoader::load() noexcept {
    return pImpl_->load();
}

// -----------------------------------------------------------------------------
const cgl::Settings* SettingsLoader::settings() const noexcept {
    return pImpl_->settings();
}
