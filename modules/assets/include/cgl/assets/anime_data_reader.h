// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include "cgl/common/version.h"
#include "cgl/common/results.h"
#include "cgl/common/assets.h"

namespace cgl {

struct Settings;

class IAnimeDataReader {
 public:
    using Ptr = std::unique_ptr<cgl::IAnimeDataReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
    };

    static cgl::IAnimeDataReader::Ptr create(const CreateInfo& createInfo);

    explicit IAnimeDataReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IAnimeDataReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    virtual cgl::Results load() = 0;

    virtual cgl::Results query(
        const cgl::AnimeResourceInfo& animeResInfo,
        cgl::AnimeResourceData*       pAnimeResData) = 0;

 private:
    const CreateInfo createInfo_;
};


}   // namespace cgm
