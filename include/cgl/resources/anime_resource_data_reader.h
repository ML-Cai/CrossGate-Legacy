// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include "cgl/core/version.h"
#include "cgl/core/results.h"
#include "cgl/resources/anime_resource.h"

namespace cgl {

struct Settings;

class IAnimeResourceDataReader {
 public:
    using Ptr = std::unique_ptr<cgl::IAnimeResourceDataReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
    };

    static cgl::IAnimeResourceDataReader::Ptr create(
        const CreateInfo& createInfo);

    explicit IAnimeResourceDataReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IAnimeResourceDataReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    // Try to load specific Graphic*_*.bin data but not read data yet.
    virtual cgl::Results load() = 0;

    virtual cgl::Results query(
        const cgl::AnimeResourceInfo& animeResInfo,
        cgl::AnimeResourceData*       pAnimeResData) = 0;

 private:
    const CreateInfo createInfo_;
};


}   // namespace cgm
