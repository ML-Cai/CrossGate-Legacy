// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include "cgl/trace/logger.h"
#include "cgl/common/env.h"


// -----------------------------------------------------------------------------
// Platform-specific includes for setting/unsetting environment variables
#ifdef _MSC_VER

#define UNSET_ENV_VAR(name) do { _putenv_s(name, ""); } while(0)
#define SET_ENV_VAR(name, value) do { _putenv_s(name, value); } while(0)

#else   // !_MSC_VER

#define UNSET_ENV_VAR(name) do { unsetenv(name); } while(0)
#define SET_ENV_VAR(name, value) do { setenv(name, value, 1); } while(0)

#endif  // _MSC_VER
// -----------------------------------------------------------------------------

struct CerrRedirect {
    std::ostringstream buffer;
    std::streambuf* old;
    CerrRedirect() {
        old = std::cerr.rdbuf(buffer.rdbuf());
    }
    ~CerrRedirect() {
        std::cerr.rdbuf(old);
    }
};

TEST(trace_logger, InfoLogingTest) {
    cgl::SetLogLevel(cgl::LogLevel::Info);
    CerrRedirect redirect;

    LOGT("TEST");
    EXPECT_TRUE(redirect.buffer.str().empty());
    LOGD("TEST");
    EXPECT_TRUE(redirect.buffer.str().empty());

    LOGI("TEST");
    LOGW("TEST");
    LOGE("TEST");
    std::string output = redirect.buffer.str();
    EXPECT_NE(output.find("[Info] TEST"), std::string::npos);
    EXPECT_NE(output.find("[Warning] TEST"), std::string::npos);
    EXPECT_NE(output.find("[Error] TEST"), std::string::npos);
}

TEST(trace_logger, DebugLogingTest) {
    cgl::SetLogLevel(cgl::LogLevel::Debug);

    CerrRedirect redirect;
    LOGT("TEST");
    EXPECT_TRUE(redirect.buffer.str().empty());

    LOGD("TEST");
    LOGI("TEST");
    LOGW("TEST");
    LOGE("TEST");
    std::string output = redirect.buffer.str();
    EXPECT_NE(output.find("[Debug] TEST"), std::string::npos);
    EXPECT_NE(output.find("[Info] TEST"), std::string::npos);
    EXPECT_NE(output.find("[Warning] TEST"), std::string::npos);
    EXPECT_NE(output.find("[Error] TEST"), std::string::npos);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
