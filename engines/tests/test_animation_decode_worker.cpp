#include <gtest/gtest.h>
#include "assets/animation_decode_worker.h"
#include <chrono>
#include <future>
#include <thread>
using namespace std::chrono_literals;
namespace {
std::optional<cgl::AnimationDecodeWorker::Result> take(cgl::AnimationDecodeWorker& worker) {
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while(std::chrono::steady_clock::now()<deadline) {
        if(auto result=worker.tryTake()) return result;
        std::this_thread::sleep_for(1ms);
    }
    return std::nullopt;
}
}
TEST(AnimationDecodeWorkerTest, OneBoundedSlotAndPersistentThread) {
    std::promise<void> entered,release;
    auto gate=release.get_future().share();auto started=entered.get_future();
    cgl::AnimationDecodeWorker worker;
    std::thread::id first,second;
    ASSERT_TRUE(worker.submit([&](const auto& cancel) {
        first=std::this_thread::get_id();entered.set_value();
        while(!cancel.load() && gate.wait_for(1ms)!=std::future_status::ready) {}
        cgl::AnimationPixels p;p.rgba={1,2,3,4};return p;
    },32));
    ASSERT_EQ(started.wait_for(2s),std::future_status::ready);
    EXPECT_EQ(worker.reservedBytes(),32);
    EXPECT_FALSE(worker.tryTake());
    EXPECT_FALSE(worker.submit([](const auto&){return cgl::AnimationPixels{};},32));
    release.set_value();auto result=take(worker);ASSERT_TRUE(result);ASSERT_FALSE(result->error);
    EXPECT_EQ(result->pixels.rgba,(std::vector<uint8_t>{1,2,3,4}));EXPECT_EQ(worker.reservedBytes(),0);
    ASSERT_TRUE(worker.submit([&](const auto&){second=std::this_thread::get_id();return cgl::AnimationPixels{};},32));
    ASSERT_TRUE(take(worker));EXPECT_EQ(first,second);EXPECT_NE(first,std::this_thread::get_id());
}
TEST(AnimationDecodeWorkerTest, CancelDiscardsOldCompletionAndReleasesTaskReferences) {
    for(int iteration=0;iteration<8;++iteration) {
        std::promise<void> entered;auto started=entered.get_future();
        auto token=std::make_shared<int>(42);std::weak_ptr<int> weak=token;
        cgl::AnimationDecodeWorker worker;
        ASSERT_TRUE(worker.submit([&,keep=std::move(token)](const auto& cancel) {
            entered.set_value();while(!cancel.load()) std::this_thread::sleep_for(1ms);
            cgl::AnimationPixels p;p.rgba={1};return p;
        },32));
        ASSERT_EQ(started.wait_for(2s),std::future_status::ready);
        worker.cancel();EXPECT_EQ(worker.reservedBytes(),0);EXPECT_FALSE(worker.tryTake());EXPECT_TRUE(weak.expired());
        ASSERT_TRUE(worker.submit([](const auto&){cgl::AnimationPixels p;p.rgba={2};return p;},32));
        auto result=take(worker);ASSERT_TRUE(result);EXPECT_EQ(result->pixels.rgba,(std::vector<uint8_t>{2}));
        worker.stop();
        ASSERT_TRUE(worker.submit([](const auto&){return cgl::AnimationPixels{};},32));
        ASSERT_TRUE(take(worker));
    }
}
TEST(AnimationDecodeWorkerTest, ErrorsAndOversizedResultsReleaseReservationOnTake) {
    cgl::AnimationDecodeWorker worker;
    EXPECT_THROW(worker.submit([](const auto&){return cgl::AnimationPixels{};},0),std::invalid_argument);
    EXPECT_THROW(worker.submit([](const auto&){return cgl::AnimationPixels{};},worker.MaxBytes+1),std::invalid_argument);
    ASSERT_TRUE(worker.submit([](const auto&)->cgl::AnimationPixels {throw std::runtime_error("decode failed");},32));
    auto result=take(worker);ASSERT_TRUE(result);ASSERT_TRUE(result->error);
    EXPECT_THROW(std::rethrow_exception(result->error),std::runtime_error);EXPECT_EQ(worker.reservedBytes(),0);
    ASSERT_TRUE(worker.submit([](const auto&){cgl::AnimationPixels p;p.rgba.resize(33);return p;},32));
    result=take(worker);ASSERT_TRUE(result);EXPECT_TRUE(result->error);EXPECT_TRUE(result->pixels.rgba.empty());
    EXPECT_EQ(worker.reservedBytes(),0);
    ASSERT_TRUE(worker.submit([](const auto&){cgl::AnimationPixels p;p.rgba.reserve(64);p.rgba.resize(1);return p;},32));
    result=take(worker);ASSERT_TRUE(result);EXPECT_TRUE(result->error);EXPECT_TRUE(result->pixels.rgba.empty());
}

TEST(AnimationDecodeWorkerTest, NonblockingCancelKeepsReservationUntilCpuTaskActuallyExits) {
    cgl::AnimationDecodeWorker worker;
    std::promise<void> entered,release;auto started=entered.get_future();auto gate=release.get_future().share();
    auto token=std::make_shared<int>(1);std::weak_ptr<int> weak=token;
    ASSERT_TRUE(worker.submit([&,keep=std::move(token)](const auto& cancelled) {
        entered.set_value();gate.wait();
        EXPECT_TRUE(cancelled.load());cgl::AnimationPixels p;p.rgba={1};return p;
    },32));
    const auto status=started.wait_for(2s);
    if(status!=std::future_status::ready) release.set_value();
    ASSERT_EQ(status,std::future_status::ready);
    worker.requestCancel(); // Must return before release; a blocking cancel deadlocks this test.
    EXPECT_FALSE(worker.collectCancelled());EXPECT_EQ(worker.reservedBytes(),32);EXPECT_FALSE(weak.expired());
    EXPECT_FALSE(worker.submit([](const auto&){return cgl::AnimationPixels{};},32));
    EXPECT_FALSE(worker.tryTake());release.set_value();
    const auto deadline=std::chrono::steady_clock::now()+2s;
    bool collected=false;
    while(!(collected=worker.collectCancelled()) && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    ASSERT_TRUE(collected);EXPECT_EQ(worker.reservedBytes(),0);EXPECT_TRUE(weak.expired());EXPECT_FALSE(worker.tryTake());
    ASSERT_TRUE(worker.submit([](const auto&){cgl::AnimationPixels p;p.rgba={2};return p;},32));
    auto result=take(worker);ASSERT_TRUE(result);EXPECT_EQ(result->pixels.rgba,(std::vector<uint8_t>{2}));
}
TEST(AnimationDecodeWorkerTest, RepeatedNonblockingCancelReclaimsQueuedOrRunningWork) {
    cgl::AnimationDecodeWorker worker;
    for(int i=0;i<100;++i) {
        ASSERT_TRUE(worker.submit([](const auto&){cgl::AnimationPixels p;p.rgba.resize(32);return p;},32));
        worker.requestCancel();worker.requestCancel();
        const auto deadline=std::chrono::steady_clock::now()+2s;
        while(!worker.collectCancelled() && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
        EXPECT_EQ(worker.reservedBytes(),0);EXPECT_FALSE(worker.tryTake());
    }
}
