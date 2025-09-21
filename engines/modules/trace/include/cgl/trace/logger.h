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
            ss << CGL_LOG_PREFIX << tag << __VA_ARGS__ << std::endl;    \
            std::cerr << ss.str();                                      \
        }                                                               \
    } while(false)

#define CGL_LOG_PREFIX "CGL:"
#ifndef CGL_LOG_DISABLE_COLORED_FORMATTER
#define CGL_LOG_TAG_TRACE "\033[35m[Trace] \033[0m"
#define CGL_LOG_TAG_DEBUG "\033[34m[Debug] \033[0m"
#define CGL_LOG_TAG_INFO "[Info] \033[0m"
#define CGL_LOG_TAG_WARNING "\033[33m[Warning] \033[0m"
#define CGL_LOG_TAG_ERROR "\033[31m[Error] \033[0m"
#else
#define CGL_LOG_TAG_TRACE "[Trace] "
#define CGL_LOG_TAG_DEBUG "[Debug] "
#define CGL_LOG_TAG_INFO "[Info] "
#define CGL_LOG_TAG_WARNING "[Warning] "
#define CGL_LOG_TAG_ERROR "[Error] "
#endif

#define LOGT(...) CGL_LOG_HELPER(Trace, CGL_LOG_TAG_TRACE, __VA_ARGS__)
#define LOGD(...) CGL_LOG_HELPER(Debug, CGL_LOG_TAG_DEBUG, __VA_ARGS__)
#define LOGI(...) CGL_LOG_HELPER(Info, CGL_LOG_TAG_INFO ,__VA_ARGS__)
#define LOGW(...) CGL_LOG_HELPER(Warning, CGL_LOG_TAG_WARNING, __VA_ARGS__)
#define LOGE(...) CGL_LOG_HELPER(Error, CGL_LOG_TAG_ERROR, __VA_ARGS__)

}   // namespace cgl

