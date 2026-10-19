#pragma once

#include "map/map_camera.h"
#include "cgl/graphics/sprite_renderer.h"
#include <numeric>
#include <queue>

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
inline size_t SortMapDrawItems(std::vector<MapDrawItem>& items) {
    std::sort(items.begin(),items.end(),MapDrawLess);
    const size_t n=items.size();
    std::vector<size_t> sweep(n); std::iota(sweep.begin(),sweep.end(),0);
    struct Rect { double left,right,top,bottom; };
    std::vector<Rect> rects; rects.reserve(n);
    for(const auto& item:items) {
        const auto p=MapToWorld(item.cell);
        rects.push_back({p.x+item.offsetX,p.x+item.offsetX+item.source.width,
                         p.y+item.offsetY,p.y+item.offsetY+item.source.height});
    }
    std::sort(sweep.begin(),sweep.end(),[&](size_t a,size_t b) {
        return rects[a].left!=rects[b].left ? rects[a].left<rects[b].left : a<b;
    });
    std::vector<std::vector<size_t>> after(n);
    std::vector<size_t> incoming(n,0);
    for(size_t i=0;i<n;++i) for(size_t j=i+1;j<n;++j) {
        const auto a=sweep[i],b=sweep[j];
        const auto& ra=rects[a];const auto& rb=rects[b];
        if(rb.left>=ra.right) break;
        if(ra.top>=rb.bottom || rb.top>=ra.bottom) continue;
        if(items[a].tileX<=1 && items[a].tileY<=1 &&
           items[b].tileX<=1 && items[b].tileY<=1) continue;
        const auto fa=GroundFootprint(items[a]),fb=GroundFootprint(items[b]);
        const bool aBehind=fa.minX>=fb.maxX || fa.maxY<=fb.minY;
        const bool bBehind=fb.minX>=fa.maxX || fb.maxY<=fa.minY;
        if(aBehind==bBehind) continue; // Side-by-side or intersecting: ambiguous.
        const auto first=aBehind?a:b,last=aBehind?b:a;
        after[first].push_back(last); ++incoming[last];
    }
    std::priority_queue<size_t,std::vector<size_t>,std::greater<size_t>> ready;
    for(size_t i=0;i<n;++i) if(!incoming[i]) ready.push(i);
    std::vector<bool> emitted(n,false);
    std::vector<MapDrawItem> ordered;ordered.reserve(n);
    size_t cycleBreaks=0,next=0;
    while(ordered.size()<n) {
        if(ready.empty()) {
            while(emitted[next]) ++next;
            ready.push(next); ++cycleBreaks;
        }
        const auto i=ready.top();ready.pop();
        if(emitted[i]) continue;
        emitted[i]=true;ordered.push_back(std::move(items[i]));
        for(const auto j:after[i]) if(!emitted[j] && --incoming[j]==0) ready.push(j);
    }
    items=std::move(ordered);
    return cycleBreaks;
}

inline bool MapImageVisible(MapPoint position, uint32_t width, uint32_t height,
                             double scale, const Viewport& viewport) noexcept {
    return position.x < viewport.width && position.y < viewport.height &&
        position.x + width * scale > 0 && position.y + height * scale > 0;
}
}
