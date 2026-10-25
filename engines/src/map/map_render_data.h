#pragma once

#include "map/map_camera.h"
#include "cgl/graphics/sprite_renderer.h"
#include <numeric>
#include <limits>
#include "map/map_ready_indices.h"

namespace cgl {
// Classic map placeholders/debug markers are not scene artwork.
inline bool IsClassicMapImage(uint16_t id, bool object) noexcept {
    return id != 0 && id != 2 && (!object || id < 200 || id > 265);
}
namespace component {
struct MapPosition { MapPoint cell; };
}

struct MapDrawItem {
    MapPoint cell;
    graphics::ISpriteRenderer* renderer = nullptr;
    graphics::SpriteRegion source;
    uint32_t frame = 0;
    int32_t offsetX = 0, offsetY = 0;
    uint64_t stableId = 0;
    uint8_t tileX = 1, tileY = 1; // Zero for actor point anchors.

};

// Paint back to front by ground anchor, never by the top of the image. Break
// equal-depth ties left-to-right and then by a stable object/entity identity.
inline bool MapDrawLess(const MapDrawItem& a, const MapDrawItem& b) noexcept {
    const double depthA = a.cell.y - a.cell.x, depthB = b.cell.y - b.cell.x;
    if (depthA != depthB) return depthA < depthB;
    const double sideA = a.cell.x + a.cell.y, sideB = b.cell.x + b.cell.y;
    if (sideA != sideB) return sideA < sideB;
    return a.stableId < b.stableId;
}

// Native object placement is the footprint's front/west cell. Its extent
// grows toward +MAP x (screen NE) and -MAP y (screen NW). Half-cell bounds
// distinguish adjacent cell centers; image offsets never alter ground geometry.
struct MapFootprint { double minX, maxX, minY, maxY; };
inline MapFootprint GroundFootprint(const MapDrawItem& item) noexcept {
    if (!item.tileX || !item.tileY)
        return {item.cell.x,item.cell.x,item.cell.y,item.cell.y};
    return {item.cell.x-.5,item.cell.x+item.tileX-.5,
            item.cell.y-item.tileY+.5,item.cell.y+.5};
}

// Footprint precedence is a partial order, not a valid std::sort comparator.
// Sort by the old anchor key first, then apply overlap constraints topologically.
// A screen-X sweep avoids considering pairs whose image rectangles cannot overlap.
// Ambiguous/intersecting footprints keep the baseline order where unconstrained.
// Cycles (interlocking whole sprites) break deterministically in baseline order;
// a single sprite cannot express every possible interlocking geometry.
// Scene-owned CPU scratch. Flat adjacency and an in-place permutation avoid
// per-node allocations and a second array of borrowed renderer pointers.
// Capacity follows the largest graph in this scene; clear()/destruction releases
// it. No renderer, entity, resource or asynchronous work is owned here.
class MapDrawSorter final {
public:
    size_t sort(std::vector<MapDrawItem>& items) {
        std::sort(items.begin(),items.end(),MapDrawLess);
        if(std::none_of(items.begin(),items.end(),[](const auto& item){return item.tileX>1 || item.tileY>1;})) return 0;
        const size_t n=items.size();
        sweep_.resize(n);std::iota(sweep_.begin(),sweep_.end(),0);
        rects_.clear();rects_.reserve(n);
        for(const auto& item:items) {
            const auto p=MapToWorld(item.cell);
            rects_.push_back({p.x+item.offsetX,p.x+item.offsetX+item.source.width,
                             p.y+item.offsetY,p.y+item.offsetY+item.source.height});
        }
        std::sort(sweep_.begin(),sweep_.end(),[&](size_t a,size_t b) {
            return rects_[a].left!=rects_[b].left ? rects_[a].left<rects_[b].left : a<b;
        });
        // Monotone prefix bounds skip only rectangles whose right edges are
        // all <= this object's left edge. A wide earlier sprite still participates.
        rightMax_.resize(n);double right=std::numeric_limits<double>::lowest();
        for(size_t i=0;i<n;++i) rightMax_[i]=right=std::max(right,rects_[sweep_[i]].right);
        heads_.assign(n,None);incoming_.assign(n,0);edges_.clear();
        // Preserve the overlapping pairs and footprint constraints. Dense
        // actor/actor and single-cell pairs need only the baseline anchor order.
        for(size_t a=0;a<n;++a) {
            if(items[a].tileX<=1 && items[a].tileY<=1) continue;
            const auto& ra=rects_[a];const auto fa=GroundFootprint(items[a]);
            const auto start=size_t(std::upper_bound(rightMax_.begin(),rightMax_.end(),ra.left)-rightMax_.begin());
            for(size_t k=start;k<n;++k) {
                const auto b=sweep_[k];
                const auto& rb=rects_[b];
                if(rb.left>=ra.right) break;
                if(a==b || rb.right<=ra.left || ra.top>=rb.bottom || rb.top>=ra.bottom) continue;
                if((items[b].tileX>1 || items[b].tileY>1) && b<a) continue;
                const auto fb=GroundFootprint(items[b]);
                const bool aBehind=fa.minX>=fb.maxX || fa.maxY<=fb.minY;
                const bool bBehind=fb.minX>=fa.maxX || fb.maxY<=fa.minY;
                if(aBehind==bBehind) continue;
                const auto first=aBehind?a:b,last=aBehind?b:a;
                edges_.push_back({last,heads_[first]});heads_[first]=edges_.size()-1;++incoming_[last];
            }
        }
        ready_.reset(n);order_.clear();order_.reserve(n);emitted_.assign(n,0);
        const auto push=[&](size_t i) {ready_.insert(i);};
        for(size_t i=0;i<n;++i) if(!incoming_[i]) push(i);
        size_t cycleBreaks=0,next=0;
        while(order_.size()<n) {
            if(ready_.empty()) {
                while(emitted_[next]) ++next;
                push(next);++cycleBreaks;
            }
            const auto i=ready_.takeFirst();
            if(emitted_[i]) continue;
            emitted_[i]=1;order_.push_back(i);
            for(auto e=heads_[i];e!=None;e=edges_[e].next) {
                const auto j=edges_[e].to;
                if(!emitted_[j] && --incoming_[j]==0) push(j);
            }
        }
        // order_[destination] is its old source index. Rotate each permutation
        // cycle using one item on the stack, then mark that cycle as identity.
        for(size_t i=0;i<n;++i) {
            if(order_[i]==i) continue;
            auto saved=std::move(items[i]);auto j=i;
            while(order_[j]!=i) {
                const auto source=order_[j];items[j]=std::move(items[source]);order_[j]=j;j=source;
            }
            items[j]=std::move(saved);order_[j]=j;
        }
        return cycleBreaks;
    }
    // Capacity, not live object ownership or total allocator overhead.
    size_t retainedBytes() const noexcept {
        return (sweep_.capacity()+heads_.capacity()+incoming_.capacity()+order_.capacity())*sizeof(size_t)
            +rects_.capacity()*sizeof(Rect)+rightMax_.capacity()*sizeof(double)+edges_.capacity()*sizeof(Edge)+emitted_.capacity()+ready_.retainedBytes();
    }
    void clear() noexcept { *this=MapDrawSorter{}; }
private:
    static constexpr size_t None=std::numeric_limits<size_t>::max();
    struct Rect {double left,right,top,bottom;};
    struct Edge {size_t to,next;};
    std::vector<size_t> sweep_,heads_,incoming_,order_;
    MapReadyIndices ready_;
    std::vector<Rect> rects_;
    std::vector<double> rightMax_;
    std::vector<Edge> edges_;
    std::vector<uint8_t> emitted_;
};
// Convenience for one-shot data queries/tests. Rendering retains one sorter.
inline size_t SortMapDrawItems(std::vector<MapDrawItem>& items) {
    MapDrawSorter sorter;return sorter.sort(items);
}

inline bool MapImageVisible(MapPoint position, uint32_t width, uint32_t height,
                             double scale, const Viewport& viewport) noexcept {
    return position.x < viewport.width && position.y < viewport.height &&
        position.x + width * scale > 0 && position.y + height * scale > 0;
}
}
