// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vector>
#include <future>
#include "assets/assets_components.h"

// -----------------------------------------------------------------------------
namespace cgl {

class ECSCore;

// -----------------------------------------------------------------------------
class AssetsReaderInitSystem {
 public:
    AssetsReaderInitSystem();

    ~AssetsReaderInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void initStage(cgl::ECSCore* pECS);
    void checkStage(cgl::ECSCore* pECS);
    void nopStage(cgl::ECSCore* pECS);

    using UpdateFunc = void (AssetsReaderInitSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    std::vector<std::future<
        cgl::IAnimeInfoReader::Ptr>> futures_IAnimeInfoReader_;

    std::vector<std::future<
        cgl::IAnimeDataReader::Ptr>> futures_IAnimeDataReader_;

    std::vector<std::future<
        cgl::IGraphicsDataReader::Ptr>> futures_IGraphicsDataReader_;

    std::vector<std::future<
        cgl::IGraphicsInfoReader::Ptr>> futures_IGraphicsInfoReader_;

    std::future<cgl::IPaletteReader::Ptr> futures_IPaletteReader_;

    cgl::component::AssetsReaderState* pAssetsReaderState_;
};

}   // namespace cgl
