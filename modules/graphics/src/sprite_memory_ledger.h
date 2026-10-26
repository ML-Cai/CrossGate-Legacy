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
    bool allocate(SpriteAllocationKind kind,uint64_t bytes,SpriteImageFootprint image={}) noexcept {
        auto* usage=category(kind);
        if(!validFootprint(kind,image) || !usage || !bytes || bytes>std::numeric_limits<uint64_t>::max()-stats_.total.bytes) {
            ++stats_.accountingErrors;return false;
        }
        if(kind==SpriteAllocationKind::Image) {
            auto& group=stats_.imageClasses[std::size_t(image.category)];
            if(image.payloadBytes>std::numeric_limits<uint64_t>::max()-group.payloadBytes ||
               image.frames>std::numeric_limits<uint64_t>::max()-group.frames) {++stats_.accountingErrors;return false;}
            ++group.images;group.frames+=image.frames;group.payloadBytes+=image.payloadBytes;group.allocationBytes+=bytes;
        }
        add(*usage,bytes);add(stats_.total,bytes);return true;
    }
    bool release(SpriteAllocationKind kind,uint64_t bytes,SpriteImageFootprint image={}) noexcept {
        auto* usage=category(kind);
        if(!validFootprint(kind,image) || !usage || !bytes || !usage->count || bytes>usage->bytes ||
           (usage->count==1 ? bytes!=usage->bytes : bytes==usage->bytes)) {
            ++stats_.accountingErrors;return false;
        }
        if(kind==SpriteAllocationKind::Image) {
            auto& group=stats_.imageClasses[std::size_t(image.category)];
            if(!group.images || group.frames<image.frames || group.payloadBytes<image.payloadBytes || group.allocationBytes<bytes ||
               (group.images==1 && (group.frames!=image.frames || group.payloadBytes!=image.payloadBytes || group.allocationBytes!=bytes))) {
                ++stats_.accountingErrors;return false;
            }
            --group.images;group.frames-=image.frames;group.payloadBytes-=image.payloadBytes;group.allocationBytes-=bytes;
        }
        usage->bytes-=bytes;--usage->count;stats_.total.bytes-=bytes;--stats_.total.count;return true;
    }
    SpriteAllocationStatistics snapshot() const noexcept {return stats_;}
private:
    static bool validFootprint(SpriteAllocationKind kind,const SpriteImageFootprint& image) noexcept {
        if(kind==SpriteAllocationKind::Staging) return image.payloadBytes==0 && image.frames==0 && image.category==SpriteImageClass::Unclassified;
        if(kind!=SpriteAllocationKind::Image || std::size_t(image.category)>=std::size_t(SpriteImageClass::Count)) return false;
        if(!image.payloadBytes || !image.frames) return image.payloadBytes==0 && image.frames==0 && image.category==SpriteImageClass::Unclassified;
        if(image.category==SpriteImageClass::SingleFrame) return image.frames==1;
        return image.category==SpriteImageClass::Unclassified || image.frames>1;
    }
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
