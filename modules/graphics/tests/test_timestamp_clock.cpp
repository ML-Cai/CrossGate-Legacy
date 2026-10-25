#include <gtest/gtest.h>
#include "vulkan/timestamp_clock.h"
#include <limits>
using cgl::graphics::vulkan::TimestampClock;
TEST(TimestampClock, ConvertsDeltaBeforeFloatingPointAndHandlesWrap) {
    EXPECT_DOUBLE_EQ(*(TimestampClock{64,1000}.milliseconds(UINT64_MAX-100,UINT64_MAX-90)),.01);
    EXPECT_DOUBLE_EQ(*(TimestampClock{64,1000}.milliseconds(UINT64_MAX-4,5)),.01);
    EXPECT_DOUBLE_EQ(*(TimestampClock{8,1000}.milliseconds(250,5)),.011);
    EXPECT_DOUBLE_EQ(*(TimestampClock{8,1000}.milliseconds(506,261)),.011);
    EXPECT_DOUBLE_EQ(*(TimestampClock{64,1}.milliseconds(1,1)),0);
}
TEST(TimestampClock, RejectsUnsupportedClockAndNonFiniteResults) {
    for(auto bits:{0u,65u}) EXPECT_FALSE((TimestampClock{bits,1}.milliseconds(0,1)));
    for(auto period:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        EXPECT_FALSE((TimestampClock{64,period}.milliseconds(0,1)));
    EXPECT_FALSE((TimestampClock{64,std::numeric_limits<double>::max()}.milliseconds(0,UINT64_MAX)));
}
