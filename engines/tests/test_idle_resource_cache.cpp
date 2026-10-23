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

#include "assets/deferred_resource_queue.h"
#include "render/submission_progress.h"
TEST(DeferredResourceQueue, KeepsOwnershipUntilAcknowledgedSubmissionAndHandlesZeroBytes) {
    Cache cache;cgl::DeferredResourceQueue<Resource> queue;
    cache.insert(1,std::make_shared<Resource>(1),20);
    cache.insert(2,std::make_shared<Resource>(2),0);
    std::weak_ptr<const Resource> first=cache.find(1),second=cache.find(2);
    EXPECT_EQ(cache.retire(0,[]{},[&](const auto& p,uint64_t bytes){queue.retain(7,p,bytes);},0),20);
    EXPECT_EQ(cache.size(),0);EXPECT_EQ(queue.size(),2);EXPECT_EQ(queue.bytes(),20);
    EXPECT_FALSE(first.expired());EXPECT_FALSE(second.expired());
    EXPECT_EQ(queue.collect(6),0);EXPECT_EQ(queue.size(),2);
    EXPECT_EQ(queue.collect(7),20);EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);
    EXPECT_TRUE(first.expired());EXPECT_TRUE(second.expired());EXPECT_EQ(queue.collect(99),0);
}
TEST(DeferredResourceQueue, FailedTransferKeepsCacheOwnerAndHeldResourcesStayCached) {
    Cache cache;auto pin=std::make_shared<Resource>(1);cache.insert(1,pin,10);
    cache.insert(2,std::make_shared<Resource>(2),20);
    std::weak_ptr<const Resource> weak=cache.find(2);
    EXPECT_THROW(cache.retire(0,[]{},[](const auto&,uint64_t){throw std::bad_alloc();}),std::bad_alloc);
    EXPECT_EQ(cache.size(),2);EXPECT_EQ(cache.bytes(),30);EXPECT_FALSE(weak.expired());
    cgl::DeferredResourceQueue<Resource> queue;
    EXPECT_EQ(cache.retire(0,[]{},[&](const auto& p,uint64_t bytes){queue.retain(1,p,bytes);}),20);
    EXPECT_EQ(cache.bytes(),10);EXPECT_EQ(queue.bytes(),20);EXPECT_EQ(pin->id,1);
    queue.collect(1);EXPECT_TRUE(weak.expired());
}
TEST(DeferredResourceQueue, PreservesOrderRejectsOverflowAndBoundsRepeatedCycles) {
    cgl::DeferredResourceQueue<Resource> queue;auto p=std::make_shared<Resource>(1);
    EXPECT_THROW(queue.retain(0,nullptr,1),std::invalid_argument);
    queue.retain(2,p,std::numeric_limits<uint64_t>::max());
    EXPECT_THROW(queue.retain(3,p,1),std::overflow_error);
    EXPECT_THROW(queue.retain(1,p,0),std::invalid_argument);
    EXPECT_EQ(queue.size(),1);queue.collect(2);EXPECT_EQ(queue.bytes(),0);
    for(uint64_t i=3;i<1003;++i) {
        queue.retain(i,p,4);queue.retain(i,p,0);
        EXPECT_EQ(queue.collect(i-1),0);EXPECT_EQ(queue.size(),2);
        EXPECT_EQ(queue.collect(i),4);EXPECT_EQ(queue.size(),0);
    }
    queue.retain(1003,p,4);queue.clear();EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);
}
TEST(SubmissionProgress, CompletionCannotAdvanceBeforeSuccessfulSubmissions) {
    cgl::SubmissionProgress progress;
    progress.didComplete(99);EXPECT_EQ(progress.completed,0);
    const auto first=progress.didSubmit();const auto second=progress.didSubmit();
    EXPECT_EQ(first,1);EXPECT_EQ(second,2);EXPECT_EQ(progress.completed,0);
    progress.didComplete(first);EXPECT_EQ(progress.completed,1);
    progress.didComplete(second);progress.didComplete(first);EXPECT_EQ(progress.completed,2);
    EXPECT_EQ(progress.didSubmit(),3);EXPECT_EQ(progress.completed,2);
}
