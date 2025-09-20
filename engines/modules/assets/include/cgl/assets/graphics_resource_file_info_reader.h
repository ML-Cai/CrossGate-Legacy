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

// Open the specific GraphicInfo*_*.bin to read the data.
class IGraphicsResourceFileInfoReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsResourceFileInfoReader>;

    struct CreateInfo {
        const cgl::RuntimeSettings* pSettings;
        cgl::CrossGateVersion       version;
    };

    static cgl::IGraphicsResourceFileInfoReader::Ptr create(
        const CreateInfo& createInfo);

    explicit IGraphicsResourceFileInfoReader(
        const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IGraphicsResourceFileInfoReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    virtual cgl::Results load() = 0;

    virtual size_t infoCount() const noexcept = 0;

    virtual bool mightContain(
        const cgl::GraphicsResourceSerialNum& serialNum) const noexcept = 0;

    virtual cgl::Results query(
        const cgl::GraphicsResourceSerialNum& serialNum,
        cgl::GraphicsResourceInfo*            pGfxResInfo) const noexcept = 0;

    virtual cgl::Results queryAvailableSerialNums(
        std::vector<int32_t>* pList) noexcept = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
