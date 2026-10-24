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

#include "assets/keyed_retirement_cache.h"
#include "render/submission_progress.h"
TEST(KeyedRetirementCache, KeepsOwnershipUntilAcknowledgedSubmissionAndHandlesZeroBytes) {
    Cache cache;cgl::KeyedRetirementCache<int,Resource> queue;
    cache.insert(1,std::make_shared<Resource>(1),20);
    cache.insert(2,std::make_shared<Resource>(2),0);
    std::weak_ptr<const Resource> first=cache.find(1),second=cache.find(2);
    EXPECT_EQ(cache.retire(0,[]{},[&](const auto& p,uint64_t bytes){queue.retain(p->id,7,p,bytes);},0),20);
    EXPECT_EQ(cache.size(),0);EXPECT_EQ(queue.size(),2);EXPECT_EQ(queue.bytes(),20);
    EXPECT_FALSE(first.expired());EXPECT_FALSE(second.expired());
    EXPECT_EQ(queue.collect(6),0);EXPECT_EQ(queue.size(),2);
    EXPECT_EQ(queue.collect(7),20);EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);
    EXPECT_TRUE(first.expired());EXPECT_TRUE(second.expired());EXPECT_EQ(queue.collect(99),0);
}
TEST(KeyedRetirementCache, FailedTransferKeepsCacheOwnerAndHeldResourcesStayCached) {
    Cache cache;auto pin=std::make_shared<Resource>(1);cache.insert(1,pin,10);
    cache.insert(2,std::make_shared<Resource>(2),20);
    std::weak_ptr<const Resource> weak=cache.find(2);
    EXPECT_THROW(cache.retire(0,[]{},[](const auto&,uint64_t){throw std::bad_alloc();}),std::bad_alloc);
    EXPECT_EQ(cache.size(),2);EXPECT_EQ(cache.bytes(),30);EXPECT_FALSE(weak.expired());
    cgl::KeyedRetirementCache<int,Resource> queue;
    EXPECT_EQ(cache.retire(0,[]{},[&](const auto& p,uint64_t bytes){queue.retain(p->id,1,p,bytes);}),20);
    EXPECT_EQ(cache.bytes(),10);EXPECT_EQ(queue.bytes(),20);EXPECT_EQ(pin->id,1);
    queue.collect(1);EXPECT_TRUE(weak.expired());
}
TEST(KeyedRetirementCache, PreservesOrderRejectsOverflowAndBoundsRepeatedCycles) {
    cgl::KeyedRetirementCache<int,Resource> queue;auto p=std::make_shared<Resource>(1);
    EXPECT_THROW(queue.retain(0,0,nullptr,1),std::invalid_argument);
    queue.retain(1,2,p,std::numeric_limits<uint64_t>::max());
    EXPECT_THROW(queue.retain(2,3,p,1),std::overflow_error);
    EXPECT_THROW(queue.retain(2,1,p,0),std::invalid_argument);
    EXPECT_EQ(queue.size(),1);queue.collect(2);EXPECT_EQ(queue.bytes(),0);
    for(uint64_t i=3;i<1003;++i) {
        queue.retain(1,i,p,4);queue.retain(2,i,p,0);
        EXPECT_EQ(queue.collect(i-1),0);EXPECT_EQ(queue.size(),2);
        EXPECT_EQ(queue.collect(i),4);EXPECT_EQ(queue.size(),0);
    }
    queue.retain(1,1003,p,4);queue.clear();EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);
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

TEST(KeyedRetirementCache, BoundedCollectionCountsZeroByteEntriesAndPreservesBacklog) {
    cgl::KeyedRetirementCache<int,Resource> queue;
    std::vector<std::weak_ptr<const Resource>> weak;
    for(int i=0;i<10;++i) {
        auto p=std::make_shared<Resource>(i);weak.push_back(p);queue.retain(i,5,p,i?4:0);
    }
    EXPECT_EQ(queue.collect(5,0),0);EXPECT_EQ(queue.size(),10);
    EXPECT_EQ(queue.collect(4,8),0);EXPECT_EQ(queue.size(),10);
    EXPECT_EQ(queue.collect(5,8),28);EXPECT_EQ(queue.size(),2);EXPECT_EQ(queue.bytes(),8);
    for(size_t i=0;i<weak.size();++i) EXPECT_EQ(weak[i].expired(),i<8);
    EXPECT_EQ(queue.collect(5,8),8);EXPECT_EQ(queue.size(),0);
}
TEST(IdleResourceCache, BoundedVictimsKeepRemainingOwnersAndPayloadForNextScan) {
    Cache cache;cgl::KeyedRetirementCache<int,Resource> queue;
    for(int i=0;i<12;++i) cache.insert(i,std::make_shared<Resource>(i),4);
    auto pin=cache.find(0);
    auto transfer=[&](const auto& p,uint64_t bytes){queue.retain(p->id,1,p,bytes);};
    EXPECT_EQ(cache.retire(0,[]{},transfer,0,8),32);
    EXPECT_EQ(cache.size(),4);EXPECT_EQ(queue.size(),8);EXPECT_EQ(cache.bytes()+queue.bytes(),48);
    EXPECT_NE(cache.find(0),nullptr);
    EXPECT_EQ(cache.retire(0,[]{},transfer,0,8),12);
    EXPECT_EQ(cache.size(),1);EXPECT_EQ(queue.size(),11);EXPECT_EQ(pin->id,0);
}

TEST(KeyedRetirementCache, RestoresBeforeCompletionWithoutNewPayloadAndRetiresAtLatestSerial) {
    Cache cache;cgl::KeyedRetirementCache<int,Resource> queue;
    cache.insert(7,std::make_shared<Resource>(99),10);
    std::weak_ptr<const Resource> weak=cache.find(7);
    auto retire=[&](uint64_t serial) {
        return cache.retireKeyed(0,[]{},[&](const auto& key,const auto& p,uint64_t bytes){queue.retain(key,serial,p,bytes);});
    };
    EXPECT_EQ(retire(5),10);EXPECT_EQ(cache.bytes()+queue.bytes(),10);
    EXPECT_FALSE(queue.restore(99,[](const auto&,uint64_t){ADD_FAILURE();}));
    EXPECT_TRUE(queue.restore(7,[&](const auto& p,uint64_t bytes){cache.insert(7,p,bytes);}));
    EXPECT_EQ(cache.bytes()+queue.bytes(),10);EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.restores(),1);
    EXPECT_EQ(queue.collect(5),0);EXPECT_FALSE(weak.expired());
    EXPECT_EQ(cache.find(7)->id,99);
    EXPECT_EQ(retire(9),10);EXPECT_EQ(queue.collect(5),0);EXPECT_FALSE(weak.expired());
    EXPECT_EQ(queue.collect(9),10);EXPECT_TRUE(weak.expired());
}
TEST(KeyedRetirementCache, FailedRestorePreservesOwnershipAndMiddleRestorePreservesOrder) {
    cgl::KeyedRetirementCache<int,Resource> queue;Cache cache;
    for(int i=1;i<=3;++i) queue.retain(i,i,std::make_shared<Resource>(i),4);
    EXPECT_THROW(queue.retain(2,4,std::make_shared<Resource>(9),8),std::logic_error);
    EXPECT_EQ(queue.bytes(),12);EXPECT_EQ(queue.size(),3);
    EXPECT_THROW(queue.restore(2,[](const auto&,uint64_t){throw std::bad_alloc();}),std::bad_alloc);
    EXPECT_EQ(queue.bytes(),12);EXPECT_EQ(queue.size(),3);EXPECT_EQ(queue.restores(),0);
    EXPECT_TRUE(queue.restore(2,[&](const auto& p,uint64_t bytes){cache.insert(2,p,bytes);}));
    EXPECT_EQ(queue.collect(1),4);EXPECT_EQ(queue.size(),1);EXPECT_EQ(queue.collect(2),0);
    EXPECT_EQ(queue.collect(3),4);EXPECT_EQ(queue.size(),0);EXPECT_EQ(cache.bytes(),4);
    EXPECT_FALSE(queue.restore(2,[](const auto&,uint64_t){ADD_FAILURE();}));
    queue.clear();EXPECT_EQ(queue.restores(),0);
}
struct FailingRetirementKey {
    int value;static inline bool failCopy=false;
    explicit FailingRetirementKey(int v):value(v) {}
    FailingRetirementKey(const FailingRetirementKey& other):value(other.value) {if(failCopy) throw std::bad_alloc();}
    auto operator<=>(const FailingRetirementKey&) const=default;
};
TEST(KeyedRetirementCache, IndexInsertionFailureRollsBackNewListOwnership) {
    cgl::KeyedRetirementCache<FailingRetirementKey,Resource> queue;FailingRetirementKey key{1};
    auto resource=std::make_shared<Resource>(1);std::weak_ptr<const Resource> weak=resource;
    FailingRetirementKey::failCopy=true;
    EXPECT_THROW(queue.retain(key,1,resource,10),std::bad_alloc);
    FailingRetirementKey::failCopy=false;
    EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);EXPECT_EQ(resource.use_count(),1);
    EXPECT_FALSE(queue.restore(key,[](const auto&,uint64_t){ADD_FAILURE();}));
    queue.retain(key,1,resource,10);resource.reset();EXPECT_FALSE(weak.expired());
    EXPECT_EQ(queue.collect(1),10);EXPECT_TRUE(weak.expired());
}
TEST(KeyedRetirementCache, RepeatedRestoreRetireHasBoundedOwnershipAndIndex) {
    Cache cache;cgl::KeyedRetirementCache<int,Resource> queue;
    auto resource=std::make_shared<Resource>(1);std::weak_ptr<const Resource> weak=resource;
    queue.retain(1,1,resource,4);resource.reset();
    for(uint64_t serial=2;serial<=1001;++serial) {
        ASSERT_TRUE(queue.restore(1,[&](const auto& p,uint64_t bytes){cache.insert(1,p,bytes);}));
        EXPECT_EQ(queue.size(),0);EXPECT_EQ(queue.bytes(),0);EXPECT_EQ(cache.size(),1);
        EXPECT_EQ(cache.retireKeyed(0,[]{},[&](const auto& key,const auto& p,uint64_t bytes){queue.retain(key,serial,p,bytes);}),4);
        EXPECT_EQ(cache.size(),0);EXPECT_EQ(queue.size(),1);EXPECT_EQ(queue.bytes(),4);
    }
    EXPECT_EQ(queue.restores(),1000);EXPECT_EQ(queue.collect(1000),0);EXPECT_FALSE(weak.expired());
    EXPECT_EQ(queue.collect(1001),4);EXPECT_TRUE(weak.expired());
}

TEST(IdleResourceCache, IdleByteHintTracksOwnersWithoutReleasingOrTouchingLru) {
    Cache cache;cache.insert(1,std::make_shared<Resource>(1),10);
    cache.insert(2,std::make_shared<Resource>(2),20);
    auto pin=cache.find(1);EXPECT_EQ(cache.idleBytes(),20);EXPECT_EQ(cache.bytes(),30);
    pin.reset();EXPECT_EQ(cache.idleBytes(),30);EXPECT_EQ(cache.evictions(),0);
    EXPECT_EQ(cache.trim(20,[]{}),20); // Hint did not make older key 2 newer.
    EXPECT_NE(cache.find(1),nullptr);EXPECT_EQ(cache.find(2),nullptr);
    cache.clear();EXPECT_EQ(cache.idleBytes(),0);
}
