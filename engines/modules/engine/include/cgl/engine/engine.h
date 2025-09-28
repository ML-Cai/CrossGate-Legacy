// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "string_view"

namespace cgl {

class IEngine {
 public:
    using Ptr = std::unique_ptr<IEngine>;

    static cgl::IEngine::Ptr create(const std::string_view setting_path="");

    IEngine() = default;

    virtual ~IEngine() = default;

    virtual void run() = 0;
};

}   // namespace cgl
