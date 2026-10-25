#include <gtest/gtest.h>
#include "assets/map_decode_worker.h"
#include <chrono>
#include <memory>

namespace {
template<class F> bool waitFor(F ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready()) {if(std::chrono::steady_clock::now()>=deadline) return false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    return true;
}
cgl::MapDecodeWorker::Task pixels() {
    return [](const auto&) {cgl::MapDecodeWorker::Result r;r.pixels={1,1,{1,2,3,4}};return r;};
}
}
TEST(MapDecodeWorker, CompletedOutputKeepsSlotAndReservationUntilTaken) {
    std::atomic_bool finished=false;cgl::MapDecodeWorker worker;
    ASSERT_TRUE(worker.submit([&](const auto&){auto r=pixels()(finished);finished=true;return r;},4));
    ASSERT_TRUE(waitFor([&]{return finished.load();}));EXPECT_TRUE(worker.busy());EXPECT_EQ(worker.reservedBytes(),4);
    EXPECT_FALSE(worker.submit(pixels(),4));
    std::optional<cgl::MapDecodeWorker::Result> result;
    ASSERT_TRUE(waitFor([&]{result=worker.tryTake();return bool(result);}));
    EXPECT_FALSE(result->error);EXPECT_EQ(result->pixels.rgba,(std::vector<uint8_t>{1,2,3,4}));
    EXPECT_FALSE(worker.busy());EXPECT_EQ(worker.reservedBytes(),0);
    worker.stop();EXPECT_EQ(result->pixels.rgba.size(),4);
}
TEST(MapDecodeWorker, ZeroByteMetadataOccupiesOneSlotWithoutPixels) {
    cgl::MapDecodeWorker worker;
    EXPECT_THROW(worker.submit({},0),std::invalid_argument);
    EXPECT_THROW(worker.submit(pixels(),cgl::MapDecodeWorker::MaxBytes+1),std::invalid_argument);
    ASSERT_TRUE(worker.submit([](const auto&){return cgl::MapDecodeWorker::Result{};},0));
    EXPECT_TRUE(worker.busy());EXPECT_EQ(worker.reservedBytes(),0);
    std::optional<cgl::MapDecodeWorker::Result> result;
    ASSERT_TRUE(waitFor([&]{result=worker.tryTake();return bool(result);}));
    EXPECT_FALSE(result->error);EXPECT_TRUE(result->pixels.rgba.empty());
}
TEST(MapDecodeWorker, CancellationRetainsReservationUntilBorrowedCpuAccessEnds) {
    std::atomic_bool entered=false,release=false;cgl::MapDecodeWorker worker;
    struct Release {std::atomic_bool& value;~Release(){value=true;}} guard{release};
    ASSERT_TRUE(worker.submit([&](const auto&){entered=true;while(!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));return pixels()(release);},4));
    ASSERT_TRUE(waitFor([&]{return entered.load();}));worker.requestCancel();
    EXPECT_TRUE(worker.busy());EXPECT_EQ(worker.reservedBytes(),4);EXPECT_FALSE(worker.collectCancelled());
    EXPECT_FALSE(worker.submit(pixels(),4));release=true;
    ASSERT_TRUE(waitFor([&]{return worker.collectCancelled();}));
    EXPECT_FALSE(worker.tryTake());EXPECT_FALSE(worker.busy());EXPECT_EQ(worker.reservedBytes(),0);
    ASSERT_TRUE(worker.submit(pixels(),4));std::optional<cgl::MapDecodeWorker::Result> result;
    ASSERT_TRUE(waitFor([&]{result=worker.tryTake();return bool(result);}));EXPECT_FALSE(result->error);
}
TEST(MapDecodeWorker, ErrorsAndOversizedOutputReleaseCleanlyAndPermitReuse) {
    cgl::MapDecodeWorker worker;
    for(bool oversized:{false,true}) {
        ASSERT_TRUE(worker.submit([=](const auto&)->cgl::MapDecodeWorker::Result {
            if(!oversized) throw std::runtime_error("injected decode failure");
            cgl::MapDecodeWorker::Result r;r.pixels.rgba.resize(8);return r;
        },4));
        std::optional<cgl::MapDecodeWorker::Result> result;
        ASSERT_TRUE(waitFor([&]{result=worker.tryTake();return bool(result);}));
        ASSERT_TRUE(result->error);EXPECT_THROW(std::rethrow_exception(result->error),std::runtime_error);
        EXPECT_TRUE(result->pixels.rgba.empty());EXPECT_EQ(worker.reservedBytes(),0);
    }
    ASSERT_TRUE(worker.submit(pixels(),4));std::optional<cgl::MapDecodeWorker::Result> result;
    ASSERT_TRUE(waitFor([&]{result=worker.tryTake();return bool(result);}));EXPECT_FALSE(result->error);
}
TEST(MapDecodeWorker, StopJoinsCpuAccessAndDropsCapturedOwnersBeforeReuse) {
    std::atomic_bool entered=false;cgl::MapDecodeWorker worker;
    auto owner=std::make_shared<int>(7);std::weak_ptr<int> weak=owner;
    ASSERT_TRUE(worker.submit([owner,&entered](const auto& cancelled) {
        entered=true;while(!cancelled) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        EXPECT_EQ(*owner,7);return cgl::MapDecodeWorker::Result{};
    },0));owner.reset();
    ASSERT_TRUE(waitFor([&]{return entered.load();}));EXPECT_FALSE(weak.expired());
    worker.stop();EXPECT_TRUE(weak.expired());EXPECT_FALSE(worker.busy());worker.stop();
    ASSERT_TRUE(worker.submit(pixels(),4));worker.cancel();EXPECT_EQ(worker.reservedBytes(),0);
}
