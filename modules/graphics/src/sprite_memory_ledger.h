#pragma once
#include "cgl/graphics/sprite_memory.h"
#include <algorithm>
#include <limits>
namespace cgl::graphics {
enum class SpriteAllocationKind {Image,Staging};
// Render-owner thread only. Fixed counters, no handle table or allocation.
// Record after successful allocation; release after the actual free, not retire.
class SpriteMemoryLedger {
public:
    bool allocate(SpriteAllocationKind kind,uint64_t bytes) noexcept {
        auto* usage=category(kind);
        if(!usage || !bytes || bytes>std::numeric_limits<uint64_t>::max()-stats_.total.bytes) {
            ++stats_.accountingErrors;return false;
        }
        add(*usage,bytes);add(stats_.total,bytes);return true;
    }
    bool release(SpriteAllocationKind kind,uint64_t bytes) noexcept {
        auto* usage=category(kind);
        if(!usage || !bytes || !usage->count || bytes>usage->bytes ||
           (usage->count==1 ? bytes!=usage->bytes : bytes==usage->bytes)) {
            ++stats_.accountingErrors;return false;
        }
        usage->bytes-=bytes;--usage->count;stats_.total.bytes-=bytes;--stats_.total.count;return true;
    }
    SpriteAllocationStatistics snapshot() const noexcept {return stats_;}
private:
    SpriteAllocationUsage* category(SpriteAllocationKind kind) noexcept {
        switch(kind) {case SpriteAllocationKind::Image:return &stats_.images;
        case SpriteAllocationKind::Staging:return &stats_.staging;default:return nullptr;}
    }
    static void add(SpriteAllocationUsage& usage,uint64_t bytes) noexcept {
        usage.bytes+=bytes;++usage.count;usage.peakBytes=std::max(usage.peakBytes,usage.bytes);
        usage.peakCount=std::max(usage.peakCount,usage.count);
    }
    SpriteAllocationStatistics stats_;
};
}
