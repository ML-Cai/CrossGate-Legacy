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

TEST(SpriteMemoryLedger, ImageClassesUseLogicalCanvasSizePerFrameAndSeparateSingleFrames) {
    using namespace cgl::graphics;
    EXPECT_EQ(ClassifySpriteImage(1,1,1),SpriteImageClass::SingleFrame);
    EXPECT_EQ(ClassifySpriteImage(4096,4096,1),SpriteImageClass::SingleFrame);
    EXPECT_EQ(ClassifySpriteImage(32,32,8),SpriteImageClass::FrameUpTo4KiB);
    EXPECT_EQ(ClassifySpriteImage(33,32,8),SpriteImageClass::FrameUpTo16KiB);
    EXPECT_EQ(ClassifySpriteImage(64,64,8),SpriteImageClass::FrameUpTo16KiB);
    EXPECT_EQ(ClassifySpriteImage(65,64,8),SpriteImageClass::FrameUpTo64KiB);
    EXPECT_EQ(ClassifySpriteImage(128,128,8),SpriteImageClass::FrameUpTo64KiB);
    EXPECT_EQ(ClassifySpriteImage(129,128,8),SpriteImageClass::FrameAbove64KiB);
    EXPECT_EQ(ClassifySpriteImage(UINT32_MAX,UINT32_MAX,8),SpriteImageClass::FrameAbove64KiB);
    EXPECT_EQ(ClassifySpriteImage(0,1,2),SpriteImageClass::Unclassified);
    EXPECT_EQ(ClassifySpriteImage(1,1,0),SpriteImageClass::Unclassified);
}
TEST(SpriteMemoryLedger, LiveClassTotalsPreserveAllImagesAndReleaseMatchingFootprints) {
    using namespace cgl::graphics;SpriteMemoryLedger ledger;
    const SpriteImageFootprint small{1024,4,SpriteImageClass::FrameUpTo4KiB};
    const SpriteImageFootprint large{1024*1024,1,SpriteImageClass::SingleFrame};
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,4096,small));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,2048,small));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,1024*1024,large));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,64)); // Legacy/no descriptor stays explicitly unclassified.
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Staging,4096));
    auto s=ledger.snapshot();uint64_t count=0,bytes=0,frames=0,payload=0;
    for(const auto& g:s.imageClasses) {count+=g.images;bytes+=g.allocationBytes;frames+=g.frames;payload+=g.payloadBytes;}
    EXPECT_EQ(count,s.images.count);EXPECT_EQ(bytes,s.images.bytes);EXPECT_EQ(frames,9);EXPECT_EQ(payload,1024*1024+2048);
    const auto& g=s.imageClasses[size_t(SpriteImageClass::FrameUpTo4KiB)];EXPECT_EQ(g.images,2);EXPECT_EQ(g.frames,8);EXPECT_EQ(g.payloadBytes,2048);EXPECT_EQ(g.allocationBytes,6144);
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,2048,small));ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,4096,small));
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,1024*1024,large));ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,64));
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Staging,4096));
    for(const auto& group:ledger.snapshot().imageClasses) {EXPECT_EQ(group.images,0);EXPECT_EQ(group.frames,0);EXPECT_EQ(group.payloadBytes,0);EXPECT_EQ(group.allocationBytes,0);}
    EXPECT_EQ(ledger.snapshot().total.bytes,0);EXPECT_EQ(ledger.snapshot().accountingErrors,0);
}
TEST(SpriteMemoryLedger, BadFootprintsAndOverflowCannotPartiallyMutateAccounting) {
    using namespace cgl::graphics;SpriteMemoryLedger ledger;
    const SpriteImageFootprint good{UINT64_MAX,2,SpriteImageClass::FrameAbove64KiB};
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Image,1,{4,0,SpriteImageClass::Unclassified}));
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Image,1,{4,2,SpriteImageClass::Count}));
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Staging,1,{4,1,SpriteImageClass::SingleFrame}));
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Image,1,{4,2,SpriteImageClass::SingleFrame}));
    ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,1,good));
    EXPECT_FALSE(ledger.allocate(SpriteAllocationKind::Image,1,{1,2,SpriteImageClass::FrameAbove64KiB}));
    EXPECT_EQ(ledger.snapshot().total.bytes,1);EXPECT_EQ(ledger.snapshot().images.count,1);
    EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,1,{4,2,SpriteImageClass::FrameUpTo4KiB}));
    EXPECT_FALSE(ledger.release(SpriteAllocationKind::Image,1,{4,2,SpriteImageClass::FrameAbove64KiB}));
    ASSERT_TRUE(ledger.release(SpriteAllocationKind::Image,1,good));
    EXPECT_EQ(ledger.snapshot().total.bytes,0);EXPECT_EQ(ledger.snapshot().accountingErrors,7);
}
TEST(SpriteMemoryLedger, ClassReportDistinguishesUnknownFromSupportedEmptyAndPublishesFootprints) {
    using namespace cgl::graphics;std::ostringstream out;
    cgl::WriteSpriteAllocationDetails(out,std::nullopt);
    EXPECT_NE(out.str().find("\"sprite_image_classes\":null"),std::string::npos);
    SpriteMemoryLedger ledger;ASSERT_TRUE(ledger.allocate(SpriteAllocationKind::Image,4096,{1024,4,SpriteImageClass::FrameUpTo4KiB}));
    out.str("");out.clear();cgl::WriteSpriteAllocationDetails(out,ledger.snapshot());
    EXPECT_NE(out.str().find("\"multi_frame_le_4k\":{\"images\":1,\"frames\":4,\"payload_bytes\":1024,\"allocation_bytes\":4096}"),std::string::npos);
    EXPECT_NE(out.str().find("\"unclassified\":{\"images\":0,\"frames\":0,\"payload_bytes\":0,\"allocation_bytes\":0}"),std::string::npos);
}
