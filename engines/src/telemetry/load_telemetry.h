#pragma once
#include "telemetry/frame_telemetry.h"
namespace cgl {
struct LoadSample {double totalMs=0,cpuMs=0,uploadWaitMs=0;};
struct LoadSummary {size_t count=0;uint64_t completed=0;double meanMs=0,p95Ms=0,p99Ms=0,maxMs=0,cpuMeanMs=0,uploadWaitMeanMs=0;};
// Successful background loads only. Bounded wall-time samples, not GPU timestamps
// or server-demand latency. Cancellation/failure counts are tracked separately.
class LoadTelemetry {
public:
    static constexpr size_t Capacity=256;
    bool add(LoadSample sample) noexcept {
        for(double v:{sample.totalMs,sample.cpuMs,sample.uploadWaitMs})
            if(!std::isfinite(v) || v<0) return false;
        samples_[next_]=sample;next_=(next_+1)%Capacity;count_=std::min(count_+1,Capacity);++completed_;return true;
    }
    LoadSummary summary() const {
        LoadSummary result;result.count=count_;result.completed=completed_;
        if(!count_) return result;
        std::array<double,Capacity> values{};
        for(size_t i=0;i<count_;++i) {
            values[i]=samples_[i].totalMs;result.cpuMeanMs+=samples_[i].cpuMs;result.uploadWaitMeanMs+=samples_[i].uploadWaitMs;
        }
        auto span=std::span<double>(values.data(),count_);result.meanMs=TimingMean(span);
        std::sort(span.begin(),span.end());result.p95Ms=TimingPercentileSorted(span,.95);
        result.p99Ms=TimingPercentileSorted(span,.99);result.maxMs=span.back();
        result.cpuMeanMs/=count_;result.uploadWaitMeanMs/=count_;return result;
    }
    void clear() noexcept {count_=next_=0;completed_=0;}
private:
    std::array<LoadSample,Capacity> samples_{};size_t count_=0,next_=0;uint64_t completed_=0;
};
}
