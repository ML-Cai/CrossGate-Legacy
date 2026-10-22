// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include <mutex>
#include <string>
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
    // Engine CPU queries share this lock. Stop/join decode work before replacing
    // or destroying readers; the reader implementations themselves are not safe
    // for concurrent access. Initialization finishes before any worker submission.
    std::shared_ptr<std::mutex> queryMutex = std::make_shared<std::mutex>();

};

}   // namespace component
}   // namespace cgl
