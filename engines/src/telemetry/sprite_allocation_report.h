#pragma once
#include "cgl/graphics/sprite_memory.h"
#include <optional>
#include <ostream>
namespace cgl {
inline void WriteSpriteAllocationDetails(std::ostream& out,const std::optional<graphics::SpriteAllocationStatistics>& stats) {
    out<<",\"sprite_allocation_supported\":"<<(stats?"true":"false");
    auto write=[&](const char* name,uint64_t value) {out<<",\""<<name<<"\":";if(stats) out<<value;else out<<"null";};
    const auto s=stats.value_or(graphics::SpriteAllocationStatistics{});
    write("sprite_image_allocation_bytes",s.images.bytes);write("sprite_image_allocation_count",s.images.count);
    write("sprite_image_allocation_peak_bytes",s.images.peakBytes);write("sprite_image_allocation_peak_count",s.images.peakCount);
    write("sprite_staging_allocation_bytes",s.staging.bytes);write("sprite_staging_allocation_count",s.staging.count);
    write("sprite_staging_allocation_peak_bytes",s.staging.peakBytes);write("sprite_staging_allocation_peak_count",s.staging.peakCount);
    write("sprite_allocation_bytes",s.total.bytes);write("sprite_allocation_count",s.total.count);
    write("sprite_allocation_peak_bytes",s.total.peakBytes);write("sprite_allocation_peak_count",s.total.peakCount);
    write("sprite_allocation_accounting_errors",s.accountingErrors);
    out<<",\"sprite_image_classes\":";
    if(!stats) {out<<"null";return;}
    constexpr std::array names={"single_frame","multi_frame_le_4k","multi_frame_le_16k","multi_frame_le_64k","multi_frame_gt_64k","unclassified"};
    static_assert(names.size()==size_t(graphics::SpriteImageClass::Count));
    out<<'{';
    for(size_t i=0;i<names.size();++i) {
        if(i) out<<',';
        const auto& group=s.imageClasses[i];
        out<<'"'<<names[i]<<"\":{\"images\":"<<group.images<<",\"frames\":"<<group.frames
           <<",\"payload_bytes\":"<<group.payloadBytes<<",\"allocation_bytes\":"<<group.allocationBytes<<'}';
    }
    out<<'}';
}
}
