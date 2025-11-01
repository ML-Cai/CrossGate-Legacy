// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include <string_view>
#include "cgl/settings/engine_config.h"
#include "cgl/settings/engine_settings.h"

namespace cgl {

using SettingsPtr = std::unique_ptr<cgl::Settings>;

cgl::SettingsPtr LoadSettings(
    const std::string_view int_path="") noexcept;

}   // namespace cgl
