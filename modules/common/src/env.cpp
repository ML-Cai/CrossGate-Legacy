// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <iostream>
#include <string>
#include <cstdlib>
#include <stdexcept>
#include <charconv>
#include <algorithm>
#include "cgl/common/env.h"


// -----------------------------------------------------------------------------
bool cgl::env::exist(const std::string_view name) noexcept {
    return std::getenv(name.data()) != nullptr;
}

// -----------------------------------------------------------------------------
const uint8_t cgl::env::LogLevel() noexcept {
    // constexpr uint8_t DEFAULT_LOG_LEVEL = 2; // Info level
    constexpr uint8_t DEFAULT_LOG_LEVEL = 4; // Trace level

    static const uint8_t level = []() {
        const char* var = std::getenv(cgl::ENV_VAR_CGL_LOG_LEVEL.data());
        if (var == nullptr || *var == '\0') {
            return DEFAULT_LOG_LEVEL;
        }

        uint8_t value;
        auto [ptr, ec] = std::from_chars(var, var + std::strlen(var), value);

        if (ec == std::errc() && ptr == var + std::strlen(var)) {
            return std::max(static_cast<uint8_t>(0),
                            std::min(value, static_cast<uint8_t>(4)));
        }
        return DEFAULT_LOG_LEVEL;
    }();

    return level;
}

// -----------------------------------------------------------------------------
const std::string_view cgl::env::EngineRootPath() noexcept {
    static const std::string_view str = []() -> std::string_view {
        const char* var = std::getenv(cgl::ENV_VAR_CGL_ENGINE_ROOT_PATH.data());
        if (var != nullptr && *var != '\0') {
            return std::string_view(var);
        }
        return "";
    }();

    return str;
}
