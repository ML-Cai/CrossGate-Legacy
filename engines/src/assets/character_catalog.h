#pragma once
#include "assets/assets_components.h"
#include "assets/animation_builder.h"
#include <algorithm>
#include <mutex>

namespace cgl {
// Metadata only: no image decode or GPU work while enumerating the demo catalog.
inline std::vector<AnimationRequest> ReadCharacterCatalog(component::AssetsReaderState& readers,size_t limit=512) {
    std::vector<AnimationRequest> result;
    std::lock_guard lock(*readers.queryMutex);
    for(unsigned v=0;v<unsigned(CrossGateVersion::COUNT) && result.size()<limit;++v) {
        const auto version=CrossGateVersion(v);
        auto* infoReader=readers.pAnimeInfoReader[version].get();
        auto* dataReader=readers.pAnimeDataReader[version].get();
        auto* gfxReader=readers.pGfxInfoReader[version].get();
        if(!infoReader || !dataReader || !gfxReader) continue;
        std::vector<AnimeResourceSerialNum> serials;
        if(infoReader->queryAvailableSerialNums(&serials)!=Results::Success) continue;
        std::sort(serials.begin(),serials.end(),[](auto a,auto b){return a.value<b.value;});
        for(auto serial:serials) {
            AnimeResourceInfo info{}; AnimeResourceData data{};
            if(infoReader->query(serial,&info)!=Results::Success || dataReader->query(info,&data)!=Results::Success) continue;
            bool valid=true;
            for(unsigned d=0;d<8;++d) for(unsigned m=0;m<2;++m) {
                auto it=data.motionMap.find({DirectionTypes(d),MotionTypes(m)});
                if(it==data.motionMap.end() || !it->second.duration || it->second.motionGraphicsSerialNums.empty()) valid=false;
            }
            if(!valid) continue;
            const unsigned direction=serial.value%8;
            const auto& frames=data.motionMap.at({DirectionTypes(direction),MotionTypes(0)}).motionGraphicsSerialNums;
            if(frames.size()>256) continue;
            int64_t left=0,top=0,right=0,bottom=0;
            for(auto graphic:frames) {
                GraphicsResourceInfo frame{};
                if(gfxReader->query({GraphicsResourceSerialNumTypes::GraphicsSerialNum,version,graphic},&frame)!=Results::Success ||
                   frame.width<=0 || frame.height<=0 || frame.width>4096 || frame.height>4096) {valid=false;break;}
                {left=std::min(left,int64_t(frame.offsetX));top=std::min(top,int64_t(frame.offsetY));
                    right=std::max(right,int64_t(frame.offsetX)+frame.width);bottom=std::max(bottom,int64_t(frame.offsetY)+frame.height);}
            }
            if(!valid || right-left>4096 || bottom-top>4096 || (right-left)*(bottom-top)*frames.size()*4>32ull*1024*1024) continue;
            result.push_back({serial.value,direction,0,version});
            if(result.size()==limit) break;
        }
    }
    if(result.empty()) throw std::runtime_error("No character appearances in installed assets");
    return result;
}
}
