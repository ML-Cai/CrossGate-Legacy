#pragma once
#include "map/map_camera.h"
#include <random>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>

namespace cgl {
// CPU-only stand-in for a server's complete interest snapshot. Appearance indexes
// refer to a bounded catalog; identity and world position are independent of it.
struct CharacterPresence { uint64_t id; MapPoint cell; size_t appearance; };
struct CharacterSnapshot { uint64_t sequence=0; std::vector<CharacterPresence> actors; };
class LocalCharacterFeed {
public:
    explicit LocalCharacterFeed(size_t appearances, uint32_t seed=0xC61,size_t visible=128,size_t prefetch=160)
        : appearances_(appearances),visible_(visible),prefetch_(prefetch),random_(seed) {
        if(!appearances || visible>1024 || prefetch>1024-visible) throw std::invalid_argument("Empty character catalog");
    }
    static int region(MapPoint cell,const component::MapCamera& camera,const Viewport& viewport) {
        const auto p=MapToScreen(cell,camera,viewport);
        if(p.x>=0 && p.y>=0 && p.x<viewport.width && p.y<viewport.height) return 0;
        if(p.x>=-.25*viewport.width && p.y>=-.25*viewport.height &&
           p.x<viewport.width*1.25 && p.y<viewport.height*1.25) return 1;
        return -1;
    }
    const CharacterSnapshot& next(const component::MapCamera& camera,const Viewport& viewport,
                                  uint32_t width,uint32_t height,bool replace=false) {
        std::vector<MapPoint> cells[2];
        const auto bounds=VisibleMapBounds(camera,viewport,width,height,
            viewport.width*.25/camera.scale,viewport.height*.25/camera.scale);
        for(int y=bounds.top;y<=bounds.bottom;++y) for(int x=bounds.left;x<=bounds.right;++x) {
            MapPoint cell{double(x),double(y)};
            const int r=region(cell,camera,viewport);
            if(r>=0) cells[r].push_back(cell);
        }
        CharacterSnapshot result; result.sequence=snapshot_.sequence+1;
        const size_t targets[2]={std::min(visible_,cells[0].size()*10),std::min(prefetch_,cells[1].size()*10)};
        size_t counts[2]={};
        std::map<std::pair<int,int>,size_t> occupancy;
        if(!replace) for(const auto& actor:snapshot_.actors) {
            if(actor.cell.x<0 || actor.cell.y<0 || actor.cell.x>=width || actor.cell.y>=height) continue;
            const int r=region(actor.cell,camera,viewport);
            if(r<0 || counts[r]>=targets[r]) continue;
            result.actors.push_back(actor); ++counts[r];
            ++occupancy[{int(actor.cell.x),int(actor.cell.y)}];
        }
        std::set<size_t> usedAppearances;
        for(const auto& actor:result.actors) usedAppearances.insert(actor.appearance);
        std::vector<size_t> available;
        for(size_t i=0;i<appearances_;++i) if(!usedAppearances.contains(i)) available.push_back(i);
        std::shuffle(available.begin(),available.end(),random_);
        for(int r=0;r<2;++r) {
            std::shuffle(cells[r].begin(),cells[r].end(),random_);
            for(int layer=0;layer<10 && counts[r]<targets[r];++layer) for(const auto& cell:cells[r]) {
                auto& used=occupancy[{int(cell.x),int(cell.y)}];
                if(used>=10) continue;
                const auto appearance=available.empty() ? size_t(random_())%appearances_ : available.back();
                if(!available.empty()) available.pop_back();
                result.actors.push_back({++nextId_,cell,appearance});
                ++used;
                if(++counts[r]==targets[r]) break;
            }
        }
        snapshot_=std::move(result);
        return snapshot_;
    }
private:
    size_t appearances_,visible_,prefetch_;
    std::mt19937 random_;
    uint64_t nextId_=0;
    CharacterSnapshot snapshot_;
};
}
