// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string_view>

namespace cgl {

constexpr std::string_view ENV_VAR_CGL_LOG_LEVEL        = "CGL_LOG_LEVEL";
constexpr std::string_view ENV_VAR_CGL_ENGINE_ROOT_PATH = "CGL_ENGINE_ROOT_PATH";


namespace env {

bool exist(const std::string_view) noexcept;

const uint8_t LogLevel() noexcept;

const std::string_view EngineRootPath()  noexcept;

}   // namespace env
}   // namespace cgl
