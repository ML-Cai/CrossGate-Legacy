#include <gtest/gtest.h>
#include "map/appearance_scheduler.h"
TEST(AppearanceScheduler, VisibleBeforePrefetchAndDuplicateDemandsUseBestRegion) {
    cgl::AppearanceScheduler s;
    EXPECT_EQ(s.update({{1,1,1},{2,0,100},{1,0,50},{3,-1,0}},0),1);
    EXPECT_EQ(s.size(),2);
    EXPECT_TRUE(s.shouldSupersede(99,1));
    EXPECT_FALSE(s.shouldSupersede(2,1)); // Same-region work stays stable.
    EXPECT_EQ(s.update({{1,1,1},{2,0,100}},1),2);
    EXPECT_TRUE(s.shouldSupersede(1,2));
}
TEST(AppearanceScheduler, AgingPreventsNearerNewcomersFromStarvingSameRegion) {
    cgl::AppearanceScheduler s;
    EXPECT_EQ(s.update({{1,0,100},{2,0,1}},0),2);
    EXPECT_EQ(s.update({{1,0,100},{3,0,1}},249),3);
    EXPECT_EQ(s.update({{1,0,100},{4,0,1}},250),1);
    EXPECT_EQ(s.update({{1,1,100},{4,0,1}},500),4); // Visible always wins.
    EXPECT_EQ(s.update({{1,1,100},{5,1,1}},501),1);
    EXPECT_EQ(s.update({{5,1,1}},502),5);
    EXPECT_FALSE(s.update({},503));EXPECT_EQ(s.size(),0);
    EXPECT_EQ(s.update({{1,1,100},{5,1,1}},504),5); // Removed demand loses age.
    s.clear();EXPECT_EQ(s.size(),0);
}
TEST(AppearanceScheduler, BoundedReplacementNeverQueuesOldSnapshots) {
    cgl::AppearanceScheduler s;
    for(size_t i=0;i<1000;++i) {
        EXPECT_EQ(s.update({{i,0,1}},i),i);EXPECT_EQ(s.size(),1);
        if(i) EXPECT_TRUE(s.shouldSupersede(i-1,i));
    }
    std::vector<cgl::AppearanceScheduler::Demand> full;
    for(size_t batch=0;batch<100;++batch) {
        full.clear();const size_t base=10000+batch*1024;
        for(size_t i=0;i<1024;++i) full.push_back({base+i,0,1});
        EXPECT_EQ(s.update(full,1000+batch),base);EXPECT_EQ(s.size(),1024);
    }
    EXPECT_THROW(s.update(std::vector<cgl::AppearanceScheduler::Demand>(1025),1100),std::invalid_argument);
    EXPECT_EQ(s.size(),1024);
}

TEST(AppearanceScheduler, BudgetMissAllowsSmallerDemandAndRecoversWithoutLosingAge) {
    cgl::AppearanceScheduler s;
    const std::vector<cgl::AppearanceScheduler::Demand> demand{{1,0,1},{2,0,10},{3,1,0}};
    EXPECT_EQ(s.update(demand,0,20),1);
    s.defer(1,30);EXPECT_TRUE(s.hasDeferred());
    EXPECT_EQ(s.select(1,20),2); // Same-region fitting demand bypasses the large one.
    s.defer(2,21);EXPECT_EQ(s.select(2,20),3);
    s.defer(3,25);EXPECT_FALSE(s.select(3,20));
    EXPECT_EQ(s.update(demand,300,30),1); // Original age and visible priority survive.
    EXPECT_EQ(s.select(301,29),2);
}
TEST(AppearanceScheduler, DeferredMetadataOnlyLivesWithCurrentUnresolvedSnapshot) {
    cgl::AppearanceScheduler s;
    EXPECT_EQ(s.update({{1,0,1}},0,1),1); // Unknown cost gets one metadata attempt.
    s.defer(1,2);EXPECT_FALSE(s.select(1,1));
    EXPECT_FALSE(s.update({},2,1));EXPECT_FALSE(s.hasDeferred());
    EXPECT_EQ(s.update({{1,0,1}},3,1),1); // Ready/removed keys lose stale cost.
    s.defer(99,100);EXPECT_FALSE(s.hasDeferred());
    s.defer(1,2);s.clear();EXPECT_FALSE(s.hasDeferred());EXPECT_EQ(s.size(),0);
}
TEST(AppearanceScheduler, ExactFitAndSharedAppearanceUseOneBudgetRequirement) {
    cgl::AppearanceScheduler s;
    s.update({{1,1,1},{1,0,20},{2,0,1}},0,0);s.defer(1,10);s.defer(2,11);
    EXPECT_EQ(s.size(),2);EXPECT_EQ(s.select(1,10),1);EXPECT_FALSE(s.select(1,9));
    EXPECT_EQ(s.select(1,11),2);
}

TEST(AppearanceScheduler, LookaheadExcludesUploadingKeyWithoutChangingAgeOrBudget) {
    cgl::AppearanceScheduler s;
    s.update({{1,0,1},{2,0,10},{3,1,0}},0,20);
    EXPECT_EQ(s.select(1,20),1);EXPECT_EQ(s.select(1,20,1),2);
    s.defer(2,21);EXPECT_EQ(s.select(2,20,1),3);
    EXPECT_EQ(s.select(300,21,1),2);
    EXPECT_EQ(s.select(301,21),1);EXPECT_EQ(s.size(),3);
    EXPECT_FALSE(s.update({},302,21));EXPECT_FALSE(s.select(303,21,1));
}
