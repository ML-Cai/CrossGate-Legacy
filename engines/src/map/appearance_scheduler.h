#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <limits>
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
    std::optional<size_t> update(const std::vector<Demand>& demands,uint64_t nowMs,uint64_t capacity=std::numeric_limits<uint64_t>::max()) {
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
        return select(nowMs,capacity);
    }
    // A temporary budget miss is not an asset failure. Remember only current
    // unresolved demand metadata; keep its age while other fitting keys progress.
    void defer(size_t appearance,uint64_t bytes) {
        if(auto it=waiting_.find(appearance);it!=waiting_.end()) it->second.requiredBytes=bytes;
    }
    bool hasDeferred() const noexcept {
        return std::any_of(waiting_.begin(),waiting_.end(),[](const auto& item){return item.second.requiredBytes.has_value();});
    }
    std::optional<size_t> select(uint64_t nowMs,uint64_t capacity,std::optional<size_t> exclude=std::nullopt) const {
        const Entry* best=nullptr;
        for(const auto& [key,entry]:waiting_) {
            if(exclude==key || (entry.requiredBytes && *entry.requiredBytes>capacity)) continue;
            if(!best || before(entry,*best,nowMs)) best=&entry;
        }
        return best?std::optional<size_t>(best->demand.appearance):std::nullopt;
    }
    bool shouldSupersede(size_t active,std::optional<size_t> candidate) const {
        const auto current=waiting_.find(active);
        if(current==waiting_.end()) return true;
        return candidate && waiting_.at(*candidate).demand.region<current->second.demand.region;
    }
    bool contains(size_t appearance) const noexcept {return waiting_.contains(appearance);}
    bool blocked(size_t appearance,uint64_t capacity) const noexcept {
        const auto it=waiting_.find(appearance);
        return it!=waiting_.end() && it->second.requiredBytes && *it->second.requiredBytes>capacity;
    }
    size_t blockedCount(uint64_t capacity) const noexcept {
        return std::count_if(waiting_.begin(),waiting_.end(),[&](const auto& item) {
            return item.second.requiredBytes && *item.second.requiredBytes>capacity;
        });
    }
    size_t size() const noexcept {return waiting_.size();}
    void clear() noexcept {waiting_.clear();}
private:
    struct Entry {Demand demand;uint64_t since;bool seen;std::optional<uint64_t> requiredBytes;};
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
