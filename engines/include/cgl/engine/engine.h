// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include <cstdint>
#include "string_view"

namespace cgl {

class IEngine {
 public:
    using Ptr = std::unique_ptr<IEngine>;

    static cgl::IEngine::Ptr create(const std::string_view setting_path="");

    IEngine() = default;

    virtual ~IEngine() = default;

    // A nonzero frame limit is useful for unattended rendering checks.
    virtual bool run(uint32_t frameLimit = 0) = 0;
};

}   // namespace cgl
