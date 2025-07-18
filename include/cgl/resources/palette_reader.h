// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include "cgl/core/results.h"
#include "cgl/resources/resource_types.h"

namespace cgl {

struct Settings;

class IPaletteReader {
 public:
    using Ptr = std::unique_ptr<IPaletteReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
    };

    static IPaletteReader::Ptr create(const CreateInfo& createInfo);

    explicit IPaletteReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IPaletteReader() = default;

    const CreateInfo& createInfo() const noexcept { return createInfo_; }

    // Try to open specific Graphic*_*.bin data but not read data yet.
    virtual cgl::Results read(
        cgl::EnvironmentPaletteTypes envPalette,
        cgl::PaletteData256*         pPaletteData) = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
