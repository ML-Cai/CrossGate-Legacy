// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/core/version.h"
#include "cgl/core/results.h"
#include "cgl/resources/resource_types.h"

namespace cgl {

struct Settings;

// Open the specific GraphicInfo*_*.bin to read the data.
class IGraphicsResourceFileInfoReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsResourceFileInfoReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
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
        const cgl::GraphicsResourceIndex& index) const noexcept = 0;

    virtual cgl::Results query(
        const cgl::GraphicsResourceIndex& index,
        cgl::GraphicsResourceInfo*        pGfxResInfo) const noexcept = 0;

    virtual cgl::Results queryAvailableIndexValue(
        std::vector<int32_t>* pList) noexcept = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
