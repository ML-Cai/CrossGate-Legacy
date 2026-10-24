#include <gtest/gtest.h>
#include "telemetry/appearance_demand_telemetry.h"
using cgl::DemandPhase;
using Tracker=cgl::AppearanceDemandTelemetry;

TEST(AppearanceDemandTelemetry, PartitionsQueuedDeferredAndLoadingWallTime) {
    Tracker t;
    t.observe({{1,0}},0);
    t.observe({{1,0,false,false,DemandPhase::Deferred}},100);
    t.observe({{1,0,false,false,DemandPhase::Loading}},400);
    t.observe({{1,0,true}},450);
    auto s=t.summary();EXPECT_EQ(s.started,1);EXPECT_EQ(s.completed,1);EXPECT_EQ(s.pending,0);
    EXPECT_EQ(s.samples,1);EXPECT_DOUBLE_EQ(s.meanMs,450);EXPECT_DOUBLE_EQ(s.p99Ms,450);
    EXPECT_DOUBLE_EQ(s.queuedMeanMs,100);EXPECT_DOUBLE_EQ(s.deferredMeanMs,300);EXPECT_DOUBLE_EQ(s.loadingMeanMs,50);
    EXPECT_DOUBLE_EQ(s.visibleMeanMs,450);EXPECT_EQ(s.visibleCompleted,1);
}
TEST(AppearanceDemandTelemetry, SharedAppearancesUseBestRegionAndCacheHitsAreNotMissSamples) {
    Tracker t;t.observe({{1,1},{1,0},{2,0,true}},10);
    auto s=t.summary();EXPECT_EQ(s.pending,1);EXPECT_EQ(s.visiblePending,1);EXPECT_EQ(s.started,1);
    t.observe({{1,1,true},{1,0,true},{2,0,true}},30);
    s=t.summary();EXPECT_EQ(s.completed,1);EXPECT_DOUBLE_EQ(s.visibleMeanMs,20);
    t.observe({{1,0,true},{2,0,true}},100);EXPECT_EQ(t.summary().completed,1);
}
TEST(AppearanceDemandTelemetry, PrefetchPromotionOnlyMeasuresCurrentVisiblePeriod) {
    Tracker t;t.observe({{1,1}},0);t.observe({{1,0}},100);
    t.observe({{1,1}},150);t.observe({{1,0}},200);
    auto s=t.summary();EXPECT_DOUBLE_EQ(s.oldestPendingMs,200);EXPECT_DOUBLE_EQ(s.oldestVisibleMs,0);
    t.observe({{1,0,true}},250);s=t.summary();EXPECT_DOUBLE_EQ(s.meanMs,250);EXPECT_DOUBLE_EQ(s.visibleMeanMs,50);
    t.observe({{2,1}},300);t.observe({{2,0,true}},400);s=t.summary();
    EXPECT_EQ(s.visibleCompleted,2);EXPECT_DOUBLE_EQ(s.visibleMeanMs,25); // Ready at promotion has zero visible wait.
}
TEST(AppearanceDemandTelemetry, PendingAgeWithdrawalsFailuresAndReentryDoNotBecomeSuccesses) {
    Tracker t;t.observe({{1,0},{2,1}},0);t.observe({{1,0},{2,1}},1000);
    auto s=t.summary();EXPECT_EQ(s.completed,0);EXPECT_EQ(s.pending,2);EXPECT_DOUBLE_EQ(s.oldestPendingMs,1000);
    EXPECT_DOUBLE_EQ(s.oldestVisibleMs,1000);
    t.observe({{1,-1},{2,1,false,true}},1100);s=t.summary();
    EXPECT_DOUBLE_EQ(s.peakPendingMs,1100);EXPECT_DOUBLE_EQ(s.peakVisibleMs,1100);
    EXPECT_EQ(s.withdrawn,1);EXPECT_EQ(s.failed,1);EXPECT_EQ(s.pending,0);EXPECT_EQ(s.completed,0);
    t.observe({{1,0},{2,1,false,true}},1200);t.observe({{1,0,true},{2,1,false,true}},1210);
    s=t.summary();EXPECT_EQ(s.started,3);EXPECT_EQ(s.completed,1);EXPECT_DOUBLE_EQ(s.meanMs,10);
    EXPECT_EQ(s.started,s.completed+s.withdrawn+s.failed+s.pending);
}
TEST(AppearanceDemandTelemetry, LongRunsKeepOnly256SuccessSamplesAnd1024CurrentMisses) {
    Tracker t;for(uint64_t i=0;i<1000;++i) {t.observe({{1,0}},2*i);t.observe({{1,0,true}},2*i+1);}
    auto s=t.summary();EXPECT_EQ(s.completed,1000);EXPECT_EQ(s.samples,256);EXPECT_EQ(s.visibleSamples,256);
    EXPECT_DOUBLE_EQ(s.meanMs,1);
    std::vector<cgl::AppearanceObservation> observations;
    for(size_t batch=0;batch<20;++batch) {
        observations.clear();for(size_t i=0;i<1024;++i) observations.push_back({batch*1024+i,0});
        t.observe(observations,2000+batch);EXPECT_EQ(t.summary().pending,1024);
    }
    s=t.summary();EXPECT_EQ(s.withdrawn,19*1024);EXPECT_EQ(s.started,s.completed+s.withdrawn+s.failed+s.pending);
    t.clear();s=t.summary();EXPECT_EQ(s.started,0);EXPECT_EQ(s.samples,0);EXPECT_EQ(s.pending,0);EXPECT_DOUBLE_EQ(s.peakPendingMs,0);
    t.observe({{1,0}},0); // Scene reset accepts a fresh clock.
}
TEST(AppearanceDemandTelemetry, InvalidSizeAndBackwardsClockDoNotMutateState) {
    Tracker t;t.observe({{1,0}},100);
    EXPECT_THROW(t.observe({},99),std::invalid_argument);
    EXPECT_THROW(t.observe(std::vector<cgl::AppearanceObservation>(1025),200),std::invalid_argument);
    EXPECT_EQ(t.summary().pending,1);EXPECT_EQ(t.summary().withdrawn,0);
    t.observe({{1,0,true}},110);EXPECT_DOUBLE_EQ(t.summary().meanMs,10);
}
