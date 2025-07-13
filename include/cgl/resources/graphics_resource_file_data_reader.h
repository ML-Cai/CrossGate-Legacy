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
#include "cgl/resources/resource_types.h"

namespace cgl {

struct Settings;

class IGraphicsResourceFileDataReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsResourceFileDataReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
    };

    static cgl::IGraphicsResourceFileDataReader::Ptr create(
        const CreateInfo& createInfo);

    explicit IGraphicsResourceFileDataReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IGraphicsResourceFileDataReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    // Try to load specific Graphic*_*.bin data but not read data yet.
    virtual cgl::Results load() = 0;

    virtual cgl::Results query(
        const cgl::GraphicsResourceInfo& gfxResInfo,
        cgl::GraphicsResourceData*       pGfxResData) = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
