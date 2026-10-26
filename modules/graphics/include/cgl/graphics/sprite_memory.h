#pragma once
#include <cstdint>
#include <array>
namespace cgl::graphics {
enum class SpriteImageClass {SingleFrame,FrameUpTo4KiB,FrameUpTo16KiB,FrameUpTo64KiB,FrameAbove64KiB,Unclassified,Count};
inline SpriteImageClass ClassifySpriteImage(uint32_t width,uint32_t height,uint32_t frames) noexcept {
    if(!width || !height || !frames) return SpriteImageClass::Unclassified;
    if(frames==1) return SpriteImageClass::SingleFrame;
    const uint64_t pixels=uint64_t(width)*height;
    if(pixels<=1024) return SpriteImageClass::FrameUpTo4KiB;
    if(pixels<=4096) return SpriteImageClass::FrameUpTo16KiB;
    if(pixels<=16384) return SpriteImageClass::FrameUpTo64KiB;
    return SpriteImageClass::FrameAbove64KiB;
}
struct SpriteImageFootprint {
    uint64_t payloadBytes=0;uint32_t frames=0;SpriteImageClass category=SpriteImageClass::Unclassified;
};
struct SpriteImageClassUsage {uint64_t images=0,frames=0,payloadBytes=0,allocationBytes=0;};
struct SpriteAllocationUsage {
    uint64_t bytes=0,count=0,peakBytes=0,peakCount=0;
};
// Successful vkAllocateMemory allocation sizes for sprite images/staging only.
// Not physical residency or total process/device memory; no Vulkan handles.
struct SpriteAllocationStatistics {
    SpriteAllocationUsage images,staging,total;
    uint64_t accountingErrors=0;
    std::array<SpriteImageClassUsage,std::size_t(SpriteImageClass::Count)> imageClasses{};
};
}
