// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#include "utils/logger.h"

// -----------------------------------------------------------------------------
TEST(Logger, AcquireDefaultStdOutLoggerTest) {
    auto logger = cgl::AcquireDefaultStdOutLogger();
    EXPECT_NE(logger, nullptr);
}

// -----------------------------------------------------------------------------
TEST(Logger, LoggingTest) {
    EXPECT_NO_THROW({
        LOGT("Test trace log");
        LOGD("Test debug log");
        LOGI("Test info log");
        LOGW("Test warning log");
        LOGE("Test error log");
    });
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
