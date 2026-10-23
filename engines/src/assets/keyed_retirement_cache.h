#pragma once
#include <cstdint>
#include <limits>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>

namespace cgl {
// Owner-thread, one device/graphics-queue lifetime. The key index borrows list
// iterators; it owns no second resource reference. Restore transfers ownership,
// not GPU allocations, and therefore needs no fence wait or free budget.
template<class Key,class T> class KeyedRetirementCache {
    struct Entry;
    using Entries=std::list<Entry>;
    using Index=std::map<Key,typename Entries::iterator>;
    struct Entry {
        uint64_t serial,bytes;
        std::shared_ptr<const T> resource;
        typename Index::iterator index;
    };
public:
    KeyedRetirementCache()=default;
    KeyedRetirementCache(const KeyedRetirementCache&)=delete;
    KeyedRetirementCache& operator=(const KeyedRetirementCache&)=delete;
    void retain(const Key& key,uint64_t serial,const std::shared_ptr<const T>& resource,uint64_t bytes) {
        if(!resource) throw std::invalid_argument("Null retired resource");
        if(index_.contains(key)) throw std::logic_error("Duplicate retired resource key");
        if(!entries_.empty() && serial<entries_.back().serial)
            throw std::invalid_argument("Retirement serial moved backwards");
        if(bytes>std::numeric_limits<uint64_t>::max()-bytes_)
            throw std::overflow_error("Retired payload overflow");
        entries_.push_back({serial,bytes,resource,{}});
        const auto entry=std::prev(entries_.end());
        try {entry->index=index_.emplace(key,entry).first;}
        catch(...) {entries_.erase(entry);throw;}
        bytes_+=bytes;
    }
    // Receiver may insert into a destination cache, but must not mutate this
    // cache or re-enter library APIs. Failure leaves retired ownership/index intact.
    template<class Receiver> bool restore(const Key& key,Receiver&& receiver) {
        const auto found=index_.find(key);
        if(found==index_.end()) return false;
        const auto entry=found->second;
        receiver(entry->resource,entry->bytes);
        bytes_-=entry->bytes;index_.erase(found);entries_.erase(entry);++restores_;
        return true;
    }
    uint64_t collect(uint64_t completed,size_t maxEntries=std::numeric_limits<size_t>::max()) noexcept {
        uint64_t released=0;size_t count=0;
        while(count<maxEntries && !entries_.empty() && entries_.front().serial<=completed) {
            const auto entry=entries_.begin();
            released+=entry->bytes;bytes_-=entry->bytes;
            index_.erase(entry->index);entries_.erase(entry);++count;
        }
        return released;
    }
    size_t size() const noexcept {return entries_.size();}
    uint64_t bytes() const noexcept {return bytes_;}
    uint64_t restores() const noexcept {return restores_;}
    // Clear only after GPU completion, before destroying render passes/device.
    void clear() noexcept {index_.clear();entries_.clear();bytes_=restores_=0;}
private:
    Entries entries_;
    Index index_;
    uint64_t bytes_=0,restores_=0;
};
}
