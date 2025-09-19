// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
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
TEST(common_env_test, DefaultIfEnvVarNotSet) {
    UNSET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data());
    EXPECT_EQ(cgl::Env::LogLevel(), 2);
}

// -----------------------------------------------------------------------------
TEST(common_env_test, CorrectlyReadsValidValue) {
    SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "3");
    EXPECT_EQ(cgl::Env::LogLevel(), 3);
}

// -----------------------------------------------------------------------------
TEST(common_env_test, HandlesInvalidString) {
    SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "invalid");
    EXPECT_EQ(cgl::Env::LogLevel(), 2);
}

// -----------------------------------------------------------------------------
TEST(common_env_test, HandlesOutOfRangeValue) {
    SET_ENV_VAR(cgl::ENV_VAR_CGL_LOG_LEVEL.data(), "10");
    EXPECT_EQ(cgl::Env::LogLevel(), 4);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
