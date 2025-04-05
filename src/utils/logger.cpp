// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <memory>

#ifndef SPDLOG_COMPILED_LIB
#define SPDLOG_COMPILED_LIB
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#if _WIN32 & CGM_ENABLE_MSVC_LOGGER
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

#include "utils/logger.h"

// -----------------------------------------------------------------------------
// internal data
// -----------------------------------------------------------------------------
namespace {

constexpr const char* const DEFAULT_LOGGER_TAG = "cgl";

static std::shared_ptr<spdlog::logger> g_pDefaultLogger = nullptr;

// -----------------------------------------------------------------------------
cgl::Logger* initDefaultLogger() {
    std::shared_ptr<spdlog::logger> logger;

#if _WIN32 & CGM_ENABLE_MSVC_LOGGER
    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    logger = std::make_shared<spdlog::logger>(DEFAULT_LOGGER_TAG, sink);
#else
    logger = spdlog::stdout_color_mt(DEFAULT_LOGGER_TAG);
#endif

    assert(logger != nullptr);

    // assign default data for the logger
    logger->set_level(spdlog::level::debug);
    g_pDefaultLogger = logger;

    return g_pDefaultLogger.get();
}

}   // namespace

// -----------------------------------------------------------------------------
// cgl logger related data
// -----------------------------------------------------------------------------
cgl::Logger* cgl::AcquireDefaultStdOutLogger() {
    if (g_pDefaultLogger != nullptr) [[likely]] {
        return g_pDefaultLogger.get();

    } else {
        // thread-safe, one-time initializatio
        static auto* logger = initDefaultLogger();
        return logger;
    }
}
