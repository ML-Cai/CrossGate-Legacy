// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/common/results.h"
#include "cgl/common/assets.h"

namespace cgl {

struct Settings;

class IPaletteReader {
 public:
    using Ptr = std::unique_ptr<IPaletteReader>;

    struct CreateInfo {
        const cgl::Settings* pSettings;
    };

    static IPaletteReader::Ptr create(const CreateInfo& createInfo);

    explicit IPaletteReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IPaletteReader() = default;

    const CreateInfo& createInfo() const noexcept { return createInfo_; }

    // Single-instance access must be serialized. Cached values are copied out.
    // Invalidate after editing palette files or changing the resource root.
    virtual void clearCache() noexcept = 0;

    virtual cgl::Results read(
        cgl::EnvironmentPaletteTypes envPalette,
        cgl::PaletteData256*         pPaletteData) = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
