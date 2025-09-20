// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <sstream>
#include <iostream>

namespace cgl {

enum class LogLevel : uint8_t {
    Error       = 0,
    Warning     = 1,
    Info        = 2,
    Debug       = 3,
    Trace       = 4,
    Default     = cgl::LogLevel::Info,
};

// Set the global log level. This function can be used to change the log level
// but it will be overridden by the environment variable `CGL_LOG_LEVEL` if it
// is set.
// Note: This function is not thread-safe. It is recommended to call this
//        function at the start of the program before any logging is done.
void SetLogLevel(const cgl::LogLevel level);

bool IsLogging(const cgl::LogLevel level);

#define CGL_LOG_HELPER(level, tag, ...)                                 \
    do {                                                                \
        if(cgl::IsLogging(cgl::LogLevel::level)) {                      \
            std::ostringstream ss;                                      \
            ss << CGL_LOG_PREFIX ## tag << __VA_ARGS__ << std::endl;    \
            std::cerr << ss.str();                                      \
        }                                                               \
    } while(false)

#define CGL_LOG_PREFIX "CGL:"
#define LOGT(...) CGL_LOG_HELPER(Trace, "[Trace] ", __VA_ARGS__)
#define LOGD(...) CGL_LOG_HELPER(Debug, "[Debug] ",__VA_ARGS__)
#define LOGI(...) CGL_LOG_HELPER(Info, "[Info] ",__VA_ARGS__)
#define LOGW(...) CGL_LOG_HELPER(Warning, "[Warning] ",__VA_ARGS__)
#define LOGE(...) CGL_LOG_HELPER(Error, "[Error] ",__VA_ARGS__)

}   // namespace cgl

