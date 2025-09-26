// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/common/version.h"
#include "cgl/common/results.h"
#include "cgl/common/assets.h"

namespace cgl {

struct Settings;

class IGraphicsDataReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsDataReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
    };

    static cgl::IGraphicsDataReader::Ptr create(const CreateInfo& createInfo);

    explicit IGraphicsDataReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IGraphicsDataReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    virtual cgl::Results load() = 0;

    virtual cgl::Results query(
        const cgl::GraphicsResourceInfo& gfxResInfo,
        cgl::GraphicsResourceData*       pGfxResData) = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
