#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
namespace cgl {
inline double TimingMean(std::span<const double> values) {
    return values.empty()?0:std::accumulate(values.begin(),values.end(),0.0)/values.size();
}
inline double TimingPercentileSorted(std::span<const double> sorted,double p) {
    if(!std::isfinite(p) || p<=0 || p>1) throw std::invalid_argument("Invalid percentile");
    return sorted.empty()?0:sorted[size_t(std::ceil(p*sorted.size()))-1];
}
struct FrameSample {double frameMs=0,updateMs=0,renderMs=0,sortMs=0;uint32_t actors=0,drawCalls=0;};
struct FrameSummary {
    size_t count=0;uint64_t total=0;
    double fps=0,meanMs=0,p50Ms=0,p95Ms=0,p99Ms=0,maxMs=0,updateMs=0,renderMs=0,sortMs=0;
    size_t over33ms=0;
};
// Fixed storage: no allocation on push, sorting or snapshot. Summary is a rolling
// window; lifetime sample count is separate. All durations are CPU wall times.
class FrameTelemetry {
public:
    static constexpr size_t Capacity=512;
    bool add(FrameSample sample) noexcept {
        for(double v:{sample.frameMs,sample.updateMs,sample.renderMs,sample.sortMs})
            if(!std::isfinite(v) || v<0) return false;
        samples_[next_]=sample;next_=(next_+1)%Capacity;count_=std::min(count_+1,Capacity);++total_;
        return true;
    }
    FrameSummary summary() const {
        FrameSummary result;result.count=count_;result.total=total_;
        std::array<double,Capacity> frames{};
        for(size_t i=0;i<count_;++i) {
            const auto& s=samples_[i];frames[i]=s.frameMs;
            result.updateMs+=s.updateMs;result.renderMs+=s.renderMs;result.sortMs+=s.sortMs;
            if(s.frameMs>1000.0/30) ++result.over33ms;
        }
        if(!count_) return result;
        auto values=std::span<double>(frames.data(),count_);
        result.meanMs=TimingMean(values);result.fps=result.meanMs>0?1000/result.meanMs:0;
        std::sort(values.begin(),values.end());
        result.p50Ms=TimingPercentileSorted(values,.5);result.p95Ms=TimingPercentileSorted(values,.95);
        result.p99Ms=TimingPercentileSorted(values,.99);result.maxMs=values.back();
        result.updateMs/=count_;result.renderMs/=count_;result.sortMs/=count_;
        return result;
    }
    void clear() noexcept {count_=next_=0;total_=0;}
private:
    std::array<FrameSample,Capacity> samples_{};
    size_t count_=0,next_=0;uint64_t total_=0;
};
namespace component {
struct RuntimeTelemetry {FrameTelemetry frames;FrameSample rendering;};
}
}
