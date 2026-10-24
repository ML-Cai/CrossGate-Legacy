#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>
namespace cgl {
// CPU-only, one current snapshot of unresolved appearance demand. No FIFO of old
// snapshots. Region 0 is visible; region 1 is the 1.5-view prefetch area.
class AppearanceScheduler {
public:
    struct Demand {size_t appearance;int region;double distance;};
    static constexpr uint64_t AgingMs=250;
    std::optional<size_t> update(const std::vector<Demand>& demands,uint64_t nowMs) {
        if(demands.size()>1024) throw std::invalid_argument("Appearance demand exceeds snapshot bound");
        for(const auto& d:demands) if(!std::isfinite(d.distance) || d.distance<0)
            throw std::invalid_argument("Invalid appearance distance");
        // Reuse nodes for a stable snapshot instead of allocating a queue per frame.
        for(auto& [key,entry]:waiting_) entry.seen=false;
        for(const auto& d:demands) {
            if(d.region<0 || d.region>1) continue;
            auto [it,inserted]=waiting_.try_emplace(d.appearance,Entry{d,nowMs,false});
            auto& entry=it->second;
            if(!entry.seen || d.region<entry.demand.region ||
                (d.region==entry.demand.region && d.distance<entry.demand.distance)) entry.demand=d;
            entry.seen=true;
        }
        std::erase_if(waiting_,[](const auto& item){return !item.second.seen;});
        const Entry* best=nullptr;
        for(const auto& [key,entry]:waiting_) if(!best || before(entry,*best,nowMs)) best=&entry;
        return best?std::optional<size_t>(best->demand.appearance):std::nullopt;
    }
    bool shouldSupersede(size_t active,std::optional<size_t> candidate) const {
        const auto current=waiting_.find(active);
        if(current==waiting_.end()) return true;
        return candidate && waiting_.at(*candidate).demand.region<current->second.demand.region;
    }
    size_t size() const noexcept {return waiting_.size();}
    void clear() noexcept {waiting_.clear();}
private:
    struct Entry {Demand demand;uint64_t since;bool seen;};
    static bool before(const Entry& a,const Entry& b,uint64_t now) {
        if(a.demand.region!=b.demand.region) return a.demand.region<b.demand.region;
        const bool agedA=now>=a.since && now-a.since>=AgingMs;
        const bool agedB=now>=b.since && now-b.since>=AgingMs;
        if(agedA!=agedB) return agedA;
        if(agedA && a.since!=b.since) return a.since<b.since;
        if(a.demand.distance!=b.demand.distance) return a.demand.distance<b.demand.distance;
        return a.demand.appearance<b.demand.appearance;
    }
    std::map<size_t,Entry> waiting_;
};
}
