#pragma once
#include "telemetry/frame_telemetry.h"
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace cgl {
enum class DemandPhase {Queued,Deferred,Loading};
struct AppearanceObservation {
    size_t appearance;int region;bool ready=false,failed=false;
    DemandPhase phase=DemandPhase::Queued;
};
struct AppearanceDemandSummary {
    uint64_t started=0,completed=0,withdrawn=0,failed=0,visibleCompleted=0;
    size_t samples=0,visibleSamples=0,pending=0,visiblePending=0;
    double oldestPendingMs=0,oldestVisibleMs=0,peakPendingMs=0,peakVisibleMs=0;
    double meanMs=0,p95Ms=0,p99Ms=0,maxMs=0,queuedMeanMs=0,deferredMeanMs=0,loadingMeanMs=0;
    double visibleMeanMs=0,visibleP95Ms=0,visibleP99Ms=0,visibleMaxMs=0;
};
// Owner-thread, CPU-only. Tracks observed misses, not cache hits or network time.
// Phases describe the interval following an observation; readiness is sampled at
// the next scene update. Duplicate appearance users share one pending episode.
class AppearanceDemandTelemetry {
public:
    static constexpr size_t Capacity=256,MaxPending=1024;
    void observe(const std::vector<AppearanceObservation>& observations,uint64_t nowMs) {
        if(observations.size()>MaxPending) throw std::invalid_argument("Demand observation exceeds snapshot bound");
        if(lastNow_ && nowMs<*lastNow_) throw std::invalid_argument("Demand clock moved backwards");
        lastNow_=nowMs;
        for(auto& [key,e]:pending_) {
            e.seen=false;peakPending_=std::max(peakPending_,nowMs-e.since);
            if(e.visibleSince) peakVisible_=std::max(peakVisible_,nowMs-*e.visibleSince);
        }
        std::array<const AppearanceObservation*,MaxPending> additions;
        size_t additionCount=0;
        for(const auto& o:observations) if(o.region>=0 && o.region<=1) {
            if(auto it=pending_.find(o.appearance);it!=pending_.end()) merge(it->second,o);
            else if(!o.ready && !o.failed) additions[additionCount++]=&o;
        }
        // Prune before insertion, so even disjoint replacement never exceeds
        // MaxPending nodes. Nothing here owns an entity, reader or GPU object.
        std::erase_if(pending_,[&](const auto& item){if(item.second.seen) return false;++withdrawn_;return true;});
        // Only previously absent misses need a second lookup. Borrowed input
        // pointers stay on this stack and never outlive observe().
        for(size_t i=0;i<additionCount;++i) {
            const auto& o=*additions[i];
            auto [it,inserted]=pending_.try_emplace(o.appearance,Entry{nowMs,nowMs});
            if(inserted) ++started_;
            merge(it->second,o);
        }
        for(auto it=pending_.begin();it!=pending_.end();) {
            auto& e=it->second;
            const auto elapsed=nowMs-e.last;e.last=nowMs;
            if(e.phase==DemandPhase::Loading) e.loading+=elapsed;
            else if(e.phase==DemandPhase::Deferred) e.deferred+=elapsed;
            else e.queued+=elapsed;
            if(e.observed.region==0) {if(!e.visibleSince) e.visibleSince=nowMs;}
            else e.visibleSince.reset();
            if(e.observed.ready) {
                samples_[next_]={double(nowMs-e.since),double(e.queued),double(e.deferred),double(e.loading)};
                next_=(next_+1)%Capacity;count_=std::min(count_+1,Capacity);++completed_;
                if(e.visibleSince) {
                    visible_[visibleNext_]=double(nowMs-*e.visibleSince);
                    visibleNext_=(visibleNext_+1)%Capacity;visibleCount_=std::min(visibleCount_+1,Capacity);++visibleCompleted_;
                }
                it=pending_.erase(it);
            } else if(e.observed.failed) {++failed_;it=pending_.erase(it);}
            else {e.phase=e.observed.phase;++it;}
        }
    }
    // Sort bounded copies only when a report is requested, never per frame.
    AppearanceDemandSummary summary() const {
        AppearanceDemandSummary s;s.started=started_;s.completed=completed_;s.withdrawn=withdrawn_;s.failed=failed_;
        s.peakPendingMs=double(peakPending_);s.peakVisibleMs=double(peakVisible_);
        s.visibleCompleted=visibleCompleted_;s.samples=count_;s.visibleSamples=visibleCount_;s.pending=pending_.size();
        for(const auto& [key,e]:pending_) {
            s.oldestPendingMs=std::max(s.oldestPendingMs,double(*lastNow_-e.since));
            if(e.visibleSince) {++s.visiblePending;s.oldestVisibleMs=std::max(s.oldestVisibleMs,double(*lastNow_-*e.visibleSince));}
        }
        std::array<double,Capacity> totals{};
        for(size_t i=0;i<count_;++i) {
            totals[i]=samples_[i].total;s.queuedMeanMs+=samples_[i].queued;
            s.deferredMeanMs+=samples_[i].deferred;s.loadingMeanMs+=samples_[i].loading;
        }
        describe(totals,count_,s.meanMs,s.p95Ms,s.p99Ms,s.maxMs);
        if(count_) {s.queuedMeanMs/=count_;s.deferredMeanMs/=count_;s.loadingMeanMs/=count_;}
        describe(visible_,visibleCount_,s.visibleMeanMs,s.visibleP95Ms,s.visibleP99Ms,s.visibleMaxMs);
        return s;
    }
    void clear() noexcept {
        pending_.clear();lastNow_.reset();next_=count_=visibleNext_=visibleCount_=0;
        started_=completed_=withdrawn_=failed_=visibleCompleted_=peakPending_=peakVisible_=0;
    }
private:
    struct Sample {double total=0,queued=0,deferred=0,loading=0;};
    struct Entry {
        uint64_t since,last,queued=0,deferred=0,loading=0;
        std::optional<uint64_t> visibleSince;
        DemandPhase phase=DemandPhase::Queued;
        AppearanceObservation observed{};bool seen=false;
    };
    static void merge(Entry& e,const AppearanceObservation& o) {
        if(!e.seen) e.observed=o;
        else {e.observed.region=std::min(e.observed.region,o.region);e.observed.ready|=o.ready;e.observed.failed|=o.failed;}
        e.seen=true;
    }
    static void describe(std::array<double,Capacity> values,size_t count,double& mean,double& p95,double& p99,double& maximum) {
        if(!count) return;
        auto span=std::span<double>(values.data(),count);mean=TimingMean(span);std::sort(span.begin(),span.end());
        p95=TimingPercentileSorted(span,.95);p99=TimingPercentileSorted(span,.99);maximum=span.back();
    }
    std::map<size_t,Entry> pending_;
    std::optional<uint64_t> lastNow_;
    std::array<Sample,Capacity> samples_{};std::array<double,Capacity> visible_{};
    size_t next_=0,count_=0,visibleNext_=0,visibleCount_=0;
    uint64_t started_=0,completed_=0,withdrawn_=0,failed_=0,visibleCompleted_=0,peakPending_=0,peakVisible_=0;
};
}
