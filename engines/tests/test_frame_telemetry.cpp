#include <gtest/gtest.h>
#include "telemetry/frame_telemetry.h"
#include "telemetry/load_telemetry.h"
#include "benchmark/crowd_benchmark.h"
#include <limits>
TEST(FrameTelemetry, EmptyWindowAndInvalidInputs) {
    cgl::FrameTelemetry frames;
    EXPECT_EQ(frames.summary().count,0);EXPECT_DOUBLE_EQ(frames.summary().fps,0);
    EXPECT_FALSE(frames.add({-1}));
    EXPECT_FALSE(frames.add({std::numeric_limits<double>::quiet_NaN()}));
    EXPECT_FALSE(frames.add({1,std::numeric_limits<double>::infinity()}));
    EXPECT_EQ(frames.summary().total,0);
    EXPECT_THROW(cgl::TimingPercentileSorted({},0),std::invalid_argument);
    EXPECT_THROW(cgl::TimingPercentileSorted({},1.1),std::invalid_argument);
}
TEST(FrameTelemetry, MatchesBenchmarkPercentilesAndTimingMeans) {
    cgl::FrameTelemetry rolling;cgl::CrowdSamples benchmark;
    for(double value:{1,2,3,100}) {rolling.add({value,value*.5,value*.25,0});benchmark.add(value,0,0,0,0);}
    const auto s=rolling.summary();
    EXPECT_EQ(s.count,4);EXPECT_EQ(s.total,4);EXPECT_EQ(s.over33ms,1);
    EXPECT_DOUBLE_EQ(s.meanMs,cgl::CrowdSamples::mean(benchmark.frame));
    EXPECT_DOUBLE_EQ(s.p50Ms,cgl::CrowdSamples::percentile(benchmark.frame,.5));
    EXPECT_DOUBLE_EQ(s.p99Ms,cgl::CrowdSamples::percentile(benchmark.frame,.99));
    EXPECT_DOUBLE_EQ(s.fps,1000/26.5);EXPECT_DOUBLE_EQ(s.updateMs,13.25);EXPECT_DOUBLE_EQ(s.renderMs,6.625);
}
TEST(FrameTelemetry, LongRunOverwritesOldSpikesAndResetDropsHistory) {
    cgl::FrameTelemetry frames;
    frames.add({1000});
    for(size_t i=0;i<100000;++i) ASSERT_TRUE(frames.add({2,1,.5,.1}));
    const auto s=frames.summary();
    EXPECT_EQ(s.count,cgl::FrameTelemetry::Capacity);EXPECT_EQ(s.total,100001);
    EXPECT_DOUBLE_EQ(s.maxMs,2);EXPECT_DOUBLE_EQ(s.fps,500);EXPECT_EQ(s.over33ms,0);
    frames.clear();EXPECT_EQ(frames.summary().count,0);EXPECT_EQ(frames.summary().total,0);
    frames.add({10});EXPECT_DOUBLE_EQ(frames.summary().p99Ms,10);
}
TEST(FrameTelemetry, WindowContainsLatestValuesAfterWrap) {
    cgl::FrameTelemetry frames;
    for(int i=1;i<=600;++i) frames.add({double(i)});
    const auto s=frames.summary();
    EXPECT_DOUBLE_EQ(s.meanMs,(89+600)/2.0);EXPECT_DOUBLE_EQ(s.p50Ms,344);EXPECT_DOUBLE_EQ(s.maxMs,600);
}

TEST(LoadTelemetry, RejectsInvalidSamplesAndSeparatesLifetimeCountFromBoundedWindow) {
    cgl::LoadTelemetry telemetry;
    EXPECT_EQ(telemetry.summary().completed,0);EXPECT_EQ(telemetry.summary().count,0);
    EXPECT_FALSE(telemetry.add({-1,0,0}));EXPECT_FALSE(telemetry.add({1,std::numeric_limits<double>::infinity(),0}));
    EXPECT_FALSE(telemetry.add({1,0,std::numeric_limits<double>::quiet_NaN()}));
    for(int i=1;i<=258;++i) ASSERT_TRUE(telemetry.add({double(i),i*.25,i*.5}));
    const auto summary=telemetry.summary();EXPECT_EQ(summary.count,256);EXPECT_EQ(summary.completed,258);
    EXPECT_DOUBLE_EQ(summary.meanMs,130.5);EXPECT_DOUBLE_EQ(summary.p95Ms,246);EXPECT_DOUBLE_EQ(summary.p99Ms,256);
    EXPECT_DOUBLE_EQ(summary.maxMs,258);EXPECT_DOUBLE_EQ(summary.cpuMeanMs,32.625);EXPECT_DOUBLE_EQ(summary.uploadWaitMeanMs,65.25);
    telemetry.clear();EXPECT_EQ(telemetry.summary().completed,0);EXPECT_EQ(telemetry.summary().count,0);
}
