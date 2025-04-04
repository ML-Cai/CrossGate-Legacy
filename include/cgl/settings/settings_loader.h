// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/core/results.h"

namespace cgl {

struct Settings;

/**
 * @brief Setting loader to load the setting for CGL engine
 */
class SettingsLoader final {
 public:
    SettingsLoader();

    ~SettingsLoader();

    cgl::Results load() noexcept;

    const cgl::Settings* settings() const noexcept;

 private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}   // namespace cgl
