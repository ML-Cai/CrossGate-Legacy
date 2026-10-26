#include <gtest/gtest.h>
#include "sprite_memory_ledger.h"
#include "cgl/graphics/device.h"
#include "telemetry/sprite_allocation_report.h"
#include <limits>
#include <sstream>
using cgl::graphics::SpriteAllocationKind;
TEST(SpriteMemoryLedger, CombinedPeakIsSimultaneousRatherThanSumOfIndependentPeaks) {
    cgl::graphics::SpriteMemoryLedger ledger;
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,100));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Staging,80));
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Staging,80));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,200));
    auto s=ledger.snapshot();EXPECT_EQ(s.images.bytes,300);EXPECT_EQ(s.images.count,2);
    EXPECT_EQ(s.staging.bytes,0);EXPECT_EQ(s.staging.peakBytes,80);
    EXPECT_EQ(s.total.bytes,300);EXPECT_EQ(s.total.peakBytes,300);EXPECT_EQ(s.total.peakCount,2);
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,100));ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,200));
    s=ledger.snapshot();EXPECT_EQ(s.total.bytes,0);EXPECT_EQ(s.total.count,0);EXPECT_EQ(s.total.peakBytes,300);EXPECT_EQ(s.accountingErrors,0);
}
TEST(SpriteMemoryLedger, InvalidTransitionsDoNotWrapOrChangeLiveUsage) {
    cgl::graphics::SpriteMemoryLedger ledger;const auto invalid=SpriteAllocationKind(99);
    EXPECT_FALSE(ledger.allocate(invalid,1));EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Image,0));
    EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,1));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,100));
    EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,0));EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,101));
    EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,99));EXPECT_FALSE(ledger.release(invalid,1));
    EXPECT_EQ(ledger.snapshot().total.bytes,100);EXPECT_EQ(ledger.snapshot().total.count,1);
    EXPECT_EQ(ledger.snapshot().accountingErrors,7);ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,100));
}
TEST(SpriteMemoryLedger, OverflowRejectionLeavesTheExistingOwnerCharged) {
    cgl::graphics::SpriteMemoryLedger ledger;const auto max=std::numeric_limits<uint64_t>::max();
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,max));
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Staging,1));EXPECT_EQ(ledger.snapshot().total.bytes,max);
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,max));EXPECT_EQ(ledger.snapshot().total.bytes,0);
    EXPECT_EQ(ledger.snapshot().accountingErrors,1);
}
TEST(SpriteMemoryLedger, RepeatedUploadAndCancelSequencesReturnToBaseline) {
    cgl::graphics::SpriteMemoryLedger ledger;
    for(int i=0;i<100000;++i) {
        ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,4096));
        ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Staging,1024));
        ASSERT_TRUE(ledger.release(SpriteAllocationKind::Staging,1024));
        ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,4096));
    }
    auto s=ledger.snapshot();EXPECT_EQ(s.total.bytes,0);EXPECT_EQ(s.total.count,0);EXPECT_EQ(s.total.peakBytes,5120);EXPECT_EQ(s.accountingErrors,0);
}
TEST(SpriteMemoryLedger, UnsupportedMeasurementsSerializeAsNullAndSupportedZeroRemainsZero) {
    class Device final:public cgl::graphics::IDevice {
        bool getLastError(std::string*) override {return false;}
        cgl::graphics::IQueue* presentQueue() override {return nullptr;}
        cgl::graphics::IQueue* graphicsQueue() override {return nullptr;}
        cgl::Results waitIdle() override {return cgl::Results::Success;}
    } device;
    EXPECT_FALSE(device.spriteAllocationStatistics());
    std::ostringstream out;cgl::WriteSpriteAllocationDetails(out,device.spriteAllocationStatistics());
    EXPECT_NE(out.str().find("\"sprite_allocation_bytes\":null"),std::string::npos);
    EXPECT_NE(out.str().find("\"sprite_allocation_supported\":false"),std::string::npos);
    out.str("");out.clear();cgl::graphics::SpriteMemoryLedger ledger;cgl::WriteSpriteAllocationDetails(out,ledger.snapshot());
    EXPECT_NE(out.str().find("\"sprite_allocation_bytes\":0"),std::string::npos);
    EXPECT_NE(out.str().find("\"sprite_allocation_supported\":true"),std::string::npos);
}
