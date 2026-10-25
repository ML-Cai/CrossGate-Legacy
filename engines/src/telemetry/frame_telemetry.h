#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <optional>
#include <ostream>
namespace cgl {
inline double TimingMean(std::span<const double> values) {
    return values.empty()?0:std::accumulate(values.begin(),values.end(),0.0)/values.size();
}
inline double TimingPercentileSorted(std::span<const double> sorted,double p) {
    if(!std::isfinite(p) || p<=0 || p>1) throw std::invalid_argument("Invalid percentile");
    return sorted.empty()?0:sorted[size_t(std::ceil(p*sorted.size()))-1];
}
struct FrameSample {double frameMs=0,updateMs=0,renderMs=0,sortMs=0;uint32_t actors=0,drawCalls=0;double fenceWaitMs=0,recordMs=0;};
struct FrameSummary {
    size_t count=0;uint64_t total=0;
    double fps=0,meanMs=0,p50Ms=0,p95Ms=0,p99Ms=0,maxMs=0,updateMs=0,renderMs=0,sortMs=0;
    double fenceWaitMs=0,recordMs=0;
    size_t over33ms=0;
    double lifetimeFps=0,lifetimeMaxMs=0;uint64_t lifetimeOver33ms=0;
};
// Fixed storage: no allocation on push, sorting or snapshot. Summary is a rolling
// window; lifetime sample count is separate. All durations are CPU wall times.
class FrameTelemetry {
public:
    static constexpr size_t Capacity=512;
    bool add(FrameSample sample) noexcept {
        for(double v:{sample.frameMs,sample.updateMs,sample.renderMs,sample.sortMs,sample.fenceWaitMs,sample.recordMs})
            if(!std::isfinite(v) || v<0) return false;
        lifetimeMs_+=sample.frameMs;lifetimeMaxMs_=std::max(lifetimeMaxMs_,sample.frameMs);
        if(sample.frameMs>1000.0/30) ++lifetimeOver33ms_;
        samples_[next_]=sample;next_=(next_+1)%Capacity;count_=std::min(count_+1,Capacity);++total_;
        return true;
    }
    FrameSummary summary() const {
        FrameSummary result;result.count=count_;result.total=total_;
        result.lifetimeFps=lifetimeMs_>0?1000.0*total_/lifetimeMs_:0;
        result.lifetimeMaxMs=lifetimeMaxMs_;result.lifetimeOver33ms=lifetimeOver33ms_;
        std::array<double,Capacity> frames{};
        for(size_t i=0;i<count_;++i) {
            const auto& s=samples_[i];frames[i]=s.frameMs;
            result.updateMs+=s.updateMs;result.renderMs+=s.renderMs;result.sortMs+=s.sortMs;result.fenceWaitMs+=s.fenceWaitMs;result.recordMs+=s.recordMs;
            if(s.frameMs>1000.0/30) ++result.over33ms;
        }
        if(!count_) return result;
        auto values=std::span<double>(frames.data(),count_);
        result.meanMs=TimingMean(values);result.fps=result.meanMs>0?1000/result.meanMs:0;
        std::sort(values.begin(),values.end());
        result.p50Ms=TimingPercentileSorted(values,.5);result.p95Ms=TimingPercentileSorted(values,.95);
        result.p99Ms=TimingPercentileSorted(values,.99);result.maxMs=values.back();
        result.updateMs/=count_;result.renderMs/=count_;result.sortMs/=count_;result.fenceWaitMs/=count_;result.recordMs/=count_;
        return result;
    }
    void clear() noexcept {count_=next_=0;total_=0;lifetimeMs_=lifetimeMaxMs_=0;lifetimeOver33ms_=0;}
private:
    std::array<FrameSample,Capacity> samples_{};
    size_t count_=0,next_=0;uint64_t total_=0,lifetimeOver33ms_=0;double lifetimeMs_=0,lifetimeMaxMs_=0;
};
struct GpuSummary {size_t count=0;uint64_t total=0,missing=0;double meanMs=0,p95Ms=0,maxMs=0;};
// Delayed completion samples, independent of CPU frame indices. No GPU ownership.
class GpuTelemetry {
public:
    static constexpr size_t Capacity=512;
    void add(std::optional<double> ms) noexcept {
        if(!ms || !std::isfinite(*ms) || *ms<0) {++missing_;return;}
        samples_[next_]=*ms;next_=(next_+1)%Capacity;count_=std::min(count_+1,Capacity);++total_;
    }
    GpuSummary summary() const {
        GpuSummary result{count_,total_,missing_};
        if(!count_) return result;
        auto copy=samples_;auto values=std::span<double>(copy.data(),count_);
        result.meanMs=TimingMean(values);std::sort(values.begin(),values.end());
        result.p95Ms=TimingPercentileSorted(values,.95);result.maxMs=values.back();return result;
    }
    void clear() noexcept {next_=count_=0;total_=missing_=0;}
private:
    std::array<double,Capacity> samples_{};size_t next_=0,count_=0;uint64_t total_=0,missing_=0;
};
namespace component {
struct RuntimeTelemetry {FrameTelemetry frames;FrameSample rendering;GpuTelemetry gpu;uint32_t gpuSlots=0;};
}
inline void WriteTimingDetails(std::ostream& out,const component::RuntimeTelemetry* telemetry) {
    const auto cpu=telemetry?telemetry->frames.summary():FrameSummary{};
    const auto gpu=telemetry?telemetry->gpu.summary():GpuSummary{};
    out << ",\"cpu_timing_samples\":" << cpu.count << ",\"cpu_fence_wait_mean_ms\":";
    if(cpu.count) out<<cpu.fenceWaitMs;else out<<"null";
    out << ",\"cpu_record_mean_ms\":";if(cpu.count) out<<cpu.recordMs;else out<<"null";
    out << ",\"gpu_timing_requested\":" << (telemetry?"true":"false")
        << ",\"gpu_supported_slots\":" << (telemetry?telemetry->gpuSlots:0)
        << ",\"gpu_window_samples\":" << gpu.count << ",\"gpu_total_samples\":" << gpu.total
        << ",\"gpu_missing_results\":" << gpu.missing << ",\"gpu_mean_ms\":";
    if(gpu.count) out<<gpu.meanMs;else out<<"null";
    out << ",\"gpu_p95_ms\":";if(gpu.count) out<<gpu.p95Ms;else out<<"null";
    out << ",\"gpu_max_ms\":";if(gpu.count) out<<gpu.maxMs;else out<<"null";
}
}
