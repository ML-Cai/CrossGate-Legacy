#pragma once
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>

namespace cgl {
// Owner-thread queue. Serial refers to successful submissions on ONE graphics
// queue, never CPU frame number, presentation completion or upload completion.
template<class T> class DeferredResourceQueue {
    struct Entry {uint64_t serial,bytes;std::shared_ptr<const T> resource;};
public:
    void retain(uint64_t serial,const std::shared_ptr<const T>& resource,uint64_t bytes) {
        if(!resource) throw std::invalid_argument("Null retired resource");
        if(!entries_.empty() && serial<entries_.back().serial)
            throw std::invalid_argument("Retirement serial moved backwards");
        if(bytes>std::numeric_limits<uint64_t>::max()-bytes_)
            throw std::overflow_error("Retired payload overflow");
        // Copy before cache erasure: allocation failure preserves cache ownership.
        entries_.push_back({serial,bytes,resource});bytes_+=bytes;
    }
    uint64_t collect(uint64_t completed) noexcept {
        uint64_t released=0;
        while(!entries_.empty() && entries_.front().serial<=completed) {
            released+=entries_.front().bytes;bytes_-=entries_.front().bytes;
            entries_.pop_front();
        }
        return released;
    }
    size_t size() const noexcept {return entries_.size();}
    uint64_t bytes() const noexcept {return bytes_;}
    // Only after device completion, before render-pass/device destruction.
    void clear() noexcept {entries_.clear();bytes_=0;}
private:
    std::deque<Entry> entries_;
    uint64_t bytes_=0;
};
}
