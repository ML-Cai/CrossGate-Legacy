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
#include "cgl/settings/engine_static_settings.h"
#include "cgl/settings/engine_runtime_settings.h"

namespace cgl {

using RuntimeSettingsPtr = std::unique_ptr<RuntimeSettings>;

cgl::RuntimeSettingsPtr LoadRuntimeSettings(
    const std::string_view int_path="") noexcept;

}   // namespace cgl
