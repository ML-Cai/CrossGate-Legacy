#pragma once
#include <cstdint>
namespace cgl::graphics {
struct SpriteAllocationUsage {
    uint64_t bytes=0,count=0,peakBytes=0,peakCount=0;
};
// Successful vkAllocateMemory allocation sizes for sprite images/staging only.
// Not physical residency or total process/device memory; no Vulkan handles.
struct SpriteAllocationStatistics {
    SpriteAllocationUsage images,staging,total;
    uint64_t accountingErrors=0;
};
}
