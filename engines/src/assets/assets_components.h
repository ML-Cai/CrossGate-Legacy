// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/common/states.h"
#include "cgl/common/version.h"
#include "cgl/common/enum_array.h"
#include "cgl/assets/graphics_info_reader.h"
#include "cgl/assets/graphics_data_reader.h"
#include "cgl/assets/anime_info_reader.h"
#include "cgl/assets/anime_data_reader.h"
#include "cgl/assets/palette_reader.h"

namespace cgl {
namespace component {

struct AssetsReaderState {
    cgl::StateTypes state;
    std::string lastError;

    cgl::EnumArray<cgl::CrossGateVersion,
                   cgl::IAnimeDataReader::Ptr> pAnimeDataReader;

    cgl::EnumArray<cgl::CrossGateVersion,
                   cgl::IAnimeInfoReader::Ptr> pAnimeInfoReader;

    cgl::EnumArray<cgl::CrossGateVersion,
                   cgl::IGraphicsDataReader::Ptr> pGfxDataReader;

    cgl::EnumArray<cgl::CrossGateVersion,
                   cgl::IGraphicsInfoReader::Ptr> pGfxInfoReader;

    cgl::IPaletteReader::Ptr pPaletteReader;
};

}   // namespace component
}   // namespace cgl
