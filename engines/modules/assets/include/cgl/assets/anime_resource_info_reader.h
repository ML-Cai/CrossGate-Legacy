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

struct RuntimeSettings;

// Open the specific AnimeInfo*_*.bin to read the data.
class IAnimeResourceInfoReader {
 public:
    using Ptr = std::unique_ptr<IAnimeResourceInfoReader>;

    struct CreateInfo {
        const cgl::RuntimeSettings* pSettings;
        cgl::CrossGateVersion       version;
    };

    static cgl::IAnimeResourceInfoReader::Ptr create(
        const CreateInfo& createInfo);

    explicit IAnimeResourceInfoReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IAnimeResourceInfoReader() = default;

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
