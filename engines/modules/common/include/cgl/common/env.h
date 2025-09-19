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

constexpr std::string_view ENV_VAR_CGL_LOG_LEVEL = "CGL_LOG_LEVEL";

// -----------------------------------------------------------------------------
class Env {
public:
    static const uint8_t LogLevel() noexcept;

private:
    Env() = delete;
};

}   // namespace cgl

