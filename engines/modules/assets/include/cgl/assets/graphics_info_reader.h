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

class IGraphicsInfoReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsInfoReader>;

    struct CreateInfo {
        const cgl::Settings* pSettings;
        cgl::CrossGateVersion       version;
    };

    static cgl::IGraphicsInfoReader::Ptr create(const CreateInfo& createInfo);

    explicit IGraphicsInfoReader(
        const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IGraphicsInfoReader() = default;

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
