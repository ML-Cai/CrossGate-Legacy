#pragma once
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace cgl {
// Frame-local ready node membership for MapDrawSorter. Each upper-level bit
// marks a nonempty word below it, preserving exact lowest-baseline-index order
// without a heap. Owns numbers only; reset reuses capacity, clear releases it.
class MapReadyIndices final {
public:
    static constexpr size_t None=std::numeric_limits<size_t>::max();
    void reset(size_t count) {
        limit_=count;levels_=0;size_t total=0;
        for(size_t words=count/64+(count%64!=0);words;words=words==1?0:words/64+(words%64!=0)) {
            offsets_[levels_++]=total;total+=words;
        }
        words_.assign(total,0);
    }
    bool empty() const noexcept {return !levels_ || !words_[offsets_[levels_-1]];}
    // Internal sorter indices must be below the count supplied to reset.
    void insert(size_t index) noexcept {
        assert(index<limit_);
        for(size_t level=0;level<levels_;++level,index/=64) {
            auto& word=words_[offsets_[level]+index/64];
            const auto previous=word;word|=uint64_t(1)<<(index%64);
            if(previous) break; // Parent already marks this word as nonempty.
        }
    }
    size_t takeFirst() noexcept {
        if(empty()) return None;
        size_t index=0;
        for(size_t level=levels_;level>0;--level)
            index=index*64+std::countr_zero(words_[offsets_[level-1]+index]);
        const auto result=index;
        for(size_t level=0;level<levels_;++level,index/=64) {
            auto& word=words_[offsets_[level]+index/64];
            word&=~(uint64_t(1)<<(index%64));
            if(word) break; // Remaining members keep the parent bit set.
        }
        return result;
    }
    size_t retainedBytes() const noexcept {return words_.capacity()*sizeof(uint64_t);}
    void clear() noexcept {*this=MapReadyIndices{};}
private:
    static constexpr size_t MaxLevels=(std::numeric_limits<size_t>::digits+5)/6;
    std::array<size_t,MaxLevels> offsets_{};
    size_t limit_=0,levels_=0;
    std::vector<uint64_t> words_;
};
}
