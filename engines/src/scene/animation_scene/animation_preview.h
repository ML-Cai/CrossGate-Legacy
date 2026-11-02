#pragma once
#include "cgl/assets/animation_decoder.h"

namespace cgl {
// Pure selection boundary: validate before touching readers or creating GPU work.
inline AnimationRequest AnimationPreviewRequest(uint32_t version,uint32_t serial,uint32_t direction,uint32_t motion) {
    if(version>=uint32_t(CrossGateVersion::COUNT))
        throw std::runtime_error("[Animation] Version must be 0..7");
    if(direction>=uint32_t(DirectionTypes::COUNT) || motion>=uint32_t(MotionTypes::COUNT))
        throw std::runtime_error("Invalid [Animation] Direction or Motion");
    return {serial,direction,motion,CrossGateVersion(version)};
}
inline double AnimationPreviewStartTime(uint32_t frame,uint32_t count,uint32_t durationMs) {
    if(!count || !durationMs || frame>=count)
        throw std::runtime_error("[Animation] StartFrame is outside the selected clip");
    // Frame zero preserves legacy phase offsets. Other frames start in their middle
    // to avoid floating-point boundary rounding selecting the previous frame.
    return frame ? (double(frame)+0.5)/count*durationMs : 0.0;
}
}
