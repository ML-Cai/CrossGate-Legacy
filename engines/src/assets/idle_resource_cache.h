#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <utility>
namespace cgl {
// Owner-thread cache policy only. GPU completion belongs to the release barrier,
// never to shared_ptr::use_count(). Entries hold no secondary strong references.
template<class Key,class T> class IdleResourceCache {
    struct Entry {std::shared_ptr<const T> value;uint64_t bytes;mutable uint64_t touched;};
    using Entries=std::map<Key,Entry>;
public:
    IdleResourceCache()=default;
    IdleResourceCache(const IdleResourceCache&)=delete;
    IdleResourceCache& operator=(const IdleResourceCache&)=delete;
    std::shared_ptr<const T> find(const Key& key) const {
        const auto it=entries_.find(key);
        if(it==entries_.end()) return nullptr;
        it->second.touched=++clock_;return it->second.value;
    }
    void insert(const Key& key,std::shared_ptr<const T> value,uint64_t bytes) {
        if(!value) throw std::invalid_argument("Null cache resource");
        if(entries_.contains(key)) return;
        if(bytes>std::numeric_limits<uint64_t>::max()-bytes_) throw std::overflow_error("Cache payload overflow");
        const auto [it,inserted]=entries_.try_emplace(key,Entry{std::move(value),bytes,++clock_});
        if(inserted) bytes_+=bytes;
    }
    size_t size() const noexcept {return entries_.size();}
    uint64_t bytes() const noexcept {return bytes_;}
    uint64_t evictions() const noexcept {return evictions_;}
    uint64_t evictedBytes() const noexcept {return evictedBytes_;}
    template<class F> void forEach(F&& f) const {for(const auto& [key,entry]:entries_) f(*entry.value);}
    // Select before waiting. A failing barrier leaves cache ownership unchanged.
    // The caller must serialize cache access; no callback may change cache membership.
    template<class Barrier> uint64_t trim(uint64_t targetBytes,Barrier&& barrier,size_t maxEntries=std::numeric_limits<size_t>::max()) {
        return retire(targetBytes,std::forward<Barrier>(barrier),[](const auto&,uint64_t){},maxEntries);
    }
    // Transfer each selected owner before erasing it. Receiver may allocate/throw;
    // that entry stays cached on failure (earlier transfers remain committed).
    template<class Barrier,class Receiver> uint64_t retire(uint64_t targetBytes,Barrier&& barrier,Receiver&& receiver,
            size_t maxEntries=std::numeric_limits<size_t>::max(),size_t maxVictims=std::numeric_limits<size_t>::max()) {
        if(bytes_<=targetBytes && entries_.size()<=maxEntries) return 0;
        std::vector<typename Entries::iterator> candidates;
        for(auto it=entries_.begin();it!=entries_.end();++it)
            if(it->second.value.use_count()==1) candidates.push_back(it);
        std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a->second.touched<b->second.touched;});
        uint64_t projected=bytes_;size_t count=0;
        while(count<maxVictims && count<candidates.size() && (projected>targetBytes || entries_.size()-count>maxEntries)) projected-=candidates[count++]->second.bytes;
        if(!count) return 0;
        barrier();
        uint64_t released=0;
        for(size_t i=0;i<count;++i) {
            const auto it=candidates[i];
            if(it->second.value.use_count()!=1) continue;
            receiver(it->second.value,it->second.bytes);
            released+=it->second.bytes;bytes_-=it->second.bytes;
            ++evictions_;evictedBytes_+=it->second.bytes;entries_.erase(it);
        }
        return released;
    }
    void clear() noexcept {entries_.clear();bytes_=clock_=evictions_=evictedBytes_=0;}
private:
    Entries entries_;
    uint64_t bytes_=0,evictions_=0,evictedBytes_=0;
    mutable uint64_t clock_=0;
};
}
