// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#ifndef SPDLOG_COMPILED_LIB
#define SPDLOG_COMPILED_LIB
#endif
#include <spdlog/spdlog.h>

namespace cgl {

using Logger = spdlog::logger;

cgl::Logger* AcquireDefaultStdOutLogger();

}   // namespace cgl


#define LOG_HELPER(LEVEL, ...)                               \
    do {                                                     \
        auto logger = cgl::AcquireDefaultStdOutLogger();     \
        if (logger) {                                        \
            logger->LEVEL(__VA_ARGS__);                      \
        }                                                    \
    } while (0)

#define LOGT(...) LOG_HELPER(trace, __VA_ARGS__)
#define LOGD(...) LOG_HELPER(debug, __VA_ARGS__)
#define LOGI(...) LOG_HELPER(info, __VA_ARGS__)
#define LOGW(...) LOG_HELPER(warn, __VA_ARGS__)
#define LOGE(...) LOG_HELPER(error, __VA_ARGS__)
