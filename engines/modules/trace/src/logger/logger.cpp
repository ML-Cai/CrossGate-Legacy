// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/common/env.h"
#include "cgl/trace/logger.h"

namespace {

static auto g_log_level = static_cast<uint8_t>(cgl::LogLevel::Default);

}   // namespace

// -----------------------------------------------------------------------------
bool cgl::IsLogging(const cgl::LogLevel level) {
    auto env_log_level = cgl::Env::LogLevel();
    auto level_ = static_cast<uint8_t>(level);

    // If the environment variable is set and valid, it takes precedence
    if(env_log_level != static_cast<uint8_t>(cgl::LogLevel::Default))
        return env_log_level >= level_;

    // Otherwise, use the global log level
    return g_log_level >= level_;
}

// -----------------------------------------------------------------------------
void cgl::SetLogLevel(const cgl::LogLevel level) {
    g_log_level = static_cast<uint8_t>(level);
}
