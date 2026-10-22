// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include <cstdlib>
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

// LogLevel caches its first value. Each assertion needs a fresh process, including
// when this executable runs all cases together. The current fallback is Trace (4).
// -----------------------------------------------------------------------------
TEST(common_env_test, DefaultIfEnvVarNotSet) {
    ASSERT_EXIT({ UNSET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data()); std::exit(cgl::env::LogLevel()==4 ? 0 : 1); }, ::testing::ExitedWithCode(0), "");
}

// -----------------------------------------------------------------------------
TEST(common_env_test, CorrectlyReadsValidValue) {
    ASSERT_EXIT({ SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "3"); std::exit(cgl::env::LogLevel()==3 ? 0 : 1); }, ::testing::ExitedWithCode(0), "");
}

// -----------------------------------------------------------------------------
TEST(common_env_test, HandlesInvalidString) {
    ASSERT_EXIT({ SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "invalid"); std::exit(cgl::env::LogLevel()==4 ? 0 : 1); }, ::testing::ExitedWithCode(0), "");
}

// -----------------------------------------------------------------------------
TEST(common_env_test, HandlesOutOfRangeValue) {
    ASSERT_EXIT({ SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "10"); std::exit(cgl::env::LogLevel()==4 ? 0 : 1); }, ::testing::ExitedWithCode(0), "");
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
