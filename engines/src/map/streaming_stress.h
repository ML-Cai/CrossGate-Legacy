#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace cgl {
inline void ValidateDemoLimits(uint32_t budget,uint32_t visible,uint32_t prefetch,uint32_t catalog) {
    if(budget<16 || budget>1024 || !visible || uint64_t(visible)+prefetch>1024 ||
        catalog<uint64_t(visible)+prefetch || catalog>4096)
        throw std::invalid_argument("Invalid ServerDemo budget/count/catalog limits");
}
struct StressMovement {double x=0,y=0,seconds=0;bool replace=false;};
// CPU-only script indexed by world updates. World movement, animation and
// snapshot time is fixed; CPU/GPU work and measured frame time stay real.
class StreamingStress {
public:
    StreamingStress(uint32_t frames,uint32_t leg,uint32_t replace,double stepMs)
        :frames_(frames),leg_(leg),replace_(replace),seconds_(stepMs/1000) {
        if(!frames || frames>1000000 || !leg || leg>6000 || !replace || replace>60000 ||
            !std::isfinite(stepMs) || stepMs<=0 || stepMs>100)
            throw std::invalid_argument("Invalid ServerDemo stress schedule");
    }
    StressMovement at(uint64_t frame) const {
        if(!frame || frame>frames_) throw std::out_of_range("Stress frame outside schedule");
        static constexpr int directions[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
        const auto direction=(frame-1)/leg_%4;
        return {double(directions[direction][0]),double(directions[direction][1]),seconds_,frame%replace_==0};
    }
    uint32_t frames() const noexcept {return frames_;}
private:
    uint32_t frames_,leg_,replace_;double seconds_;
};
namespace component {
struct StreamingDiagnostics {
    uint64_t frames=0,snapshots=0,replacements=0,budgetWaitFrames=0,visibleMissingFrames=0;
    uint64_t visibleSamples=0,readyVisibleSamples=0,peakAccountedBytes=0,peakRetiredBytes=0,peakPendingBytes=0;
    uint64_t fingerprint=14695981039346656037ull;
    uint32_t configuredFrames=0;size_t visible=0,readyVisible=0,prefetch=0,readyPrefetch=0;
    double x=0,y=0,travelCells=0;
};
}
}
