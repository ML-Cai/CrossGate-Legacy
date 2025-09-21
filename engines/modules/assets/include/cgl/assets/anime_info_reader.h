// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================


#pragma once

#include <memory>
#include <vector>
#include "cgl/common/version.h"
#include "cgl/common/results.h"
#include "cgl/common/assets.h"

namespace cgl {

struct Settings;

class IAnimeInfoReader {
 public:
    using Ptr = std::unique_ptr<IAnimeInfoReader>;

    struct CreateInfo {
        const cgl::Settings* pSettings;
        cgl::CrossGateVersion       version;
    };

    static cgl::IAnimeInfoReader::Ptr create(const CreateInfo& createInfo);

    explicit IAnimeInfoReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IAnimeInfoReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    virtual cgl::Results load() = 0;

    virtual cgl::Results query(
        const cgl::AnimeResourceSerialNum& index,
        cgl::AnimeResourceInfo*            pInfo) = 0;

    virtual cgl::Results queryAvailableSerialNums(
        std::vector<cgl::AnimeResourceSerialNum>* pList) noexcept = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgm
