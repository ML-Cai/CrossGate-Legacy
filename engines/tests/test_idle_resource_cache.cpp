#include <gtest/gtest.h>
#include "assets/idle_resource_cache.h"
struct Resource {int id;};
using Cache=cgl::IdleResourceCache<int,Resource>;
TEST(IdleResourceCache, LeastRecentlyUsedAndBarrierPrecedesDestruction) {
    Cache cache;
    cache.insert(1,std::make_shared<Resource>(1),10);
    cache.insert(2,std::make_shared<Resource>(2),10);
    cache.insert(3,std::make_shared<Resource>(3),10);
    std::weak_ptr<const Resource> oldest=cache.find(2);
    cache.find(3);cache.find(1);
    int waits=0;
    EXPECT_EQ(cache.trim(20,[&]{++waits;EXPECT_FALSE(oldest.expired());EXPECT_EQ(cache.size(),3);}),10);
    EXPECT_EQ(waits,1);EXPECT_TRUE(oldest.expired());EXPECT_FALSE(cache.find(2));
    EXPECT_EQ(cache.size(),2);EXPECT_EQ(cache.bytes(),20);EXPECT_EQ(cache.evictions(),1);
    EXPECT_EQ(cache.evictedBytes(),10);
}
TEST(IdleResourceCache, HeldResourcesAndUnderTargetDoNotWait) {
    Cache cache;auto live=std::make_shared<Resource>(1);cache.insert(1,live,100);
    EXPECT_EQ(cache.trim(0,[]{ADD_FAILURE()<<"No idle victims";}),0);
    cache.insert(2,std::make_shared<Resource>(2),10);
    EXPECT_EQ(cache.trim(110,[]{ADD_FAILURE()<<"Already below target";}),0);
    EXPECT_EQ(cache.trim(0,[]{}),10);EXPECT_EQ(cache.bytes(),100);EXPECT_EQ(live->id,1);
    live.reset();EXPECT_EQ(cache.trim(0,[]{}),100);EXPECT_EQ(cache.size(),0);
}
TEST(IdleResourceCache, FailedBarrierDoesNotEvictAndNewPinIsRechecked) {
    Cache cache;cache.insert(1,std::make_shared<Resource>(1),10);
    std::weak_ptr<const Resource> weak=cache.find(1);
    EXPECT_THROW(cache.trim(0,[]{throw std::runtime_error("GPU unavailable");}),std::runtime_error);
    EXPECT_EQ(cache.bytes(),10);EXPECT_EQ(cache.evictions(),0);EXPECT_FALSE(weak.expired());
    std::shared_ptr<const Resource> pin;
    EXPECT_EQ(cache.trim(0,[&]{pin=weak.lock();}),0);EXPECT_EQ(cache.size(),1);
    pin.reset();EXPECT_EQ(cache.trim(0,[]{}),10);EXPECT_TRUE(weak.expired());
}
TEST(IdleResourceCache, RepeatedEvictAndReloadHasBoundedMetadata) {
    Cache cache;
    for(int i=0;i<1000;++i) {
        cache.insert(i,std::make_shared<Resource>(i),4);
        EXPECT_EQ(cache.trim(0,[]{}),4);EXPECT_EQ(cache.size(),0);EXPECT_EQ(cache.bytes(),0);
    }
    EXPECT_EQ(cache.evictions(),1000);cache.clear();EXPECT_EQ(cache.evictions(),0);
    EXPECT_THROW(cache.insert(1,nullptr,10),std::invalid_argument);
    cache.insert(1,std::make_shared<Resource>(1),std::numeric_limits<uint64_t>::max());
    EXPECT_THROW(cache.insert(2,std::make_shared<Resource>(2),1),std::overflow_error);
    EXPECT_EQ(cache.size(),1);cache.clear();
}

TEST(IdleResourceCache, ZeroPayloadEntriesStillRespectCountLimit) {
    Cache cache;
    for(int i=0;i<3;++i) cache.insert(i,std::make_shared<Resource>(i),0);
    int barriers=0;
    EXPECT_EQ(cache.trim(100,[&]{++barriers;},2),0);
    EXPECT_EQ(barriers,1);EXPECT_EQ(cache.size(),2);EXPECT_FALSE(cache.find(0));
    EXPECT_EQ(cache.evictions(),1);
    cache.trim(0,[]{},0);EXPECT_EQ(cache.size(),0);
}
