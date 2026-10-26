#include <gtest/gtest.h>
#include "telemetry/frame_telemetry.h"
#include "telemetry/load_telemetry.h"
#include "benchmark/crowd_benchmark.h"
#include <limits>
#include <sstream>
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
    EXPECT_DOUBLE_EQ(s.lifetimeMaxMs,1000);EXPECT_EQ(s.lifetimeOver33ms,1);
    EXPECT_DOUBLE_EQ(s.lifetimeFps,1000.0*100001/201000);
    frames.clear();EXPECT_EQ(frames.summary().count,0);EXPECT_EQ(frames.summary().total,0);
    frames.add({10});EXPECT_DOUBLE_EQ(frames.summary().p99Ms,10);EXPECT_EQ(frames.summary().lifetimeOver33ms,0);EXPECT_DOUBLE_EQ(frames.summary().lifetimeMaxMs,10);
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

TEST(FrameTelemetry, RecordsCpuPhasesAndRejectsInvalidPhaseValues) {
    cgl::FrameTelemetry frames;
    ASSERT_TRUE(frames.add({10,1,2,0,0,0,3,4}));
    EXPECT_DOUBLE_EQ(frames.summary().fenceWaitMs,3);
    EXPECT_DOUBLE_EQ(frames.summary().recordMs,4);
    EXPECT_FALSE(frames.add({10,1,2,0,0,0,-1,4}));
    EXPECT_FALSE(frames.add({10,1,2,0,0,0,3,std::numeric_limits<double>::infinity()}));
    EXPECT_EQ(frames.summary().total,1);
}
TEST(GpuTelemetry, BoundedWindowMissingResultsAndClear) {
    cgl::GpuTelemetry gpu;
    gpu.add(std::nullopt);gpu.add(-1);gpu.add(std::numeric_limits<double>::quiet_NaN());
    for(int i=1;i<=600;++i) gpu.add(double(i));
    const auto s=gpu.summary();
    EXPECT_EQ(s.count,512);EXPECT_EQ(s.total,600);EXPECT_EQ(s.missing,3);
    EXPECT_DOUBLE_EQ(s.meanMs,344.5);EXPECT_DOUBLE_EQ(s.p95Ms,575);EXPECT_DOUBLE_EQ(s.maxMs,600);
    gpu.clear();EXPECT_EQ(gpu.summary().total,0);EXPECT_EQ(gpu.summary().missing,0);
    gpu.add(0);EXPECT_EQ(gpu.summary().count,1);EXPECT_DOUBLE_EQ(gpu.summary().meanMs,0);
}
TEST(GpuTelemetry, ReportsMissingSamplesAsNullRatherThanZero) {
    std::ostringstream out;cgl::WriteTimingDetails(out,nullptr);
    EXPECT_NE(out.str().find("\"gpu_timing_requested\":false"),std::string::npos);
    EXPECT_NE(out.str().find("\"gpu_mean_ms\":null"),std::string::npos);
    cgl::component::RuntimeTelemetry telemetry;telemetry.gpu.add(0);
    out.str("");out.clear();cgl::WriteTimingDetails(out,&telemetry);
    EXPECT_NE(out.str().find("\"gpu_mean_ms\":0"),std::string::npos);
}

TEST(SceneEntryTimings, BoundedCountersRejectInvalidSamplesAndSerializeScopes) {
    cgl::SceneEntryTimings t;
    EXPECT_FALSE(t.add(cgl::EntryStage::Count,1));EXPECT_FALSE(t.add(cgl::EntryStage::Plan,-1));
    EXPECT_FALSE(t.add(cgl::EntryStage::Plan,std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(t.add(cgl::EntryStage::Plan,std::numeric_limits<double>::quiet_NaN()));
    for(int i=0;i<100000;++i) ASSERT_TRUE(t.add(cgl::EntryStage::Catalog,2));
    EXPECT_EQ(t.stages[size_t(cgl::EntryStage::Catalog)].calls,100000);
    EXPECT_DOUBLE_EQ(t.stages[size_t(cgl::EntryStage::Catalog)].totalMs,200000);
    EXPECT_DOUBLE_EQ(t.stages[size_t(cgl::EntryStage::Catalog)].maxMs,2);
    {cgl::EntryTimer disabled(nullptr,cgl::EntryStage::Plan);}
    try {cgl::EntryTimer timer(&t,cgl::EntryStage::Plan);throw std::runtime_error("test");} catch(const std::runtime_error&) {}
    EXPECT_EQ(t.stages[size_t(cgl::EntryStage::Plan)].calls,1);
    std::ostringstream out;cgl::WriteSceneEntryTimings(out,t);
    EXPECT_NE(out.str().find("\"entry_catalog_max_ms\":2"),std::string::npos);
    EXPECT_NE(out.str().find("\"entry_remote_world_calls\":0"),std::string::npos);
}
