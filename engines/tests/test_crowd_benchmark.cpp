#include <gtest/gtest.h>
#include <set>
#include "benchmark/crowd_benchmark.h"
TEST(CrowdBenchmarkTest, ResourceCoverageAndDistinctPerCellWithSharedDivisors) {
    for (size_t cells : {1,2,5,10,605}) for(size_t unique : {1,2,10,64,512,1024}) {
        if(unique>cells*10) continue;
        std::set<size_t> all;
        for(size_t actor=0;actor<cells*10;++actor) {
            auto index=cgl::CrowdResourceIndex(actor,cells,unique);
            EXPECT_LT(index,unique); all.insert(index);
        }
        EXPECT_EQ(all.size(),unique);
        for(size_t cell=0;cell<cells;++cell) {
            std::set<size_t> local;
            for(size_t layer=0;layer<std::min(size_t(10),unique);++layer)
                local.insert(cgl::CrowdResourceIndex(cell+layer*cells,cells,unique));
            EXPECT_EQ(local.size(),std::min(size_t(10),unique));
        }
    }
    EXPECT_THROW(cgl::CrowdResourceIndex(0,0,1),std::invalid_argument);
    EXPECT_THROW(cgl::CrowdResourceIndex(0,1,0),std::invalid_argument);
}
TEST(CrowdBenchmarkTest, FiltersEffectsWithoutIdleWalkInAllDirections) {
    cgl::AnimeResourceData data{};
    EXPECT_FALSE(cgl::IsCrowdCharacterDescription(data));
    for(unsigned d=0;d<8;++d) for(unsigned m=0;m<2;++m) {
        cgl::AnimeMotionDesc desc{};desc.duration=100;desc.motionGraphicsSerialNums={1};
        data.motionMap[{cgl::DirectionTypes(d),cgl::MotionTypes(m)}]=std::move(desc);
    }
    EXPECT_TRUE(cgl::IsCrowdCharacterDescription(data));
    data.motionMap[{cgl::DirectionTypes(3),cgl::MotionTypes(1)}].motionGraphicsSerialNums.clear();
    EXPECT_FALSE(cgl::IsCrowdCharacterDescription(data));
}
TEST(CrowdBenchmarkTest, ExcludesWarmupAndUsesFrameTimeDistribution) {
    cgl::CrowdSamples s;
    for(double ms:{1000,2000,1,2,3,100}) s.add(ms,0,0,0,2);
    ASSERT_EQ(s.frame.size(),4);
    EXPECT_DOUBLE_EQ(cgl::CrowdSamples::mean(s.frame),26.5);
    EXPECT_DOUBLE_EQ(cgl::CrowdSamples::percentile(s.frame,.5),2);
    EXPECT_DOUBLE_EQ(cgl::CrowdSamples::percentile(s.frame,.99),100);
}
