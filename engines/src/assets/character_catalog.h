#pragma once
#include "assets/assets_components.h"
#include "assets/animation_builder.h"
#include <algorithm>
#include <mutex>
#include <thread>

namespace cgl {
// Metadata only: no image decode or GPU work while enumerating the demo catalog.
// Owner-thread cursor. Each poll visits at most candidateBudget serials (including
// rejected entries), and never waits for a reader held by a decode worker. Version
// enumeration/sorting and an individual reader call are still indivisible: this
// bounds work count, not wall time. Readers must remain unchanged until destruction.
class CharacterCatalogScan {
public:
    explicit CharacterCatalogScan(component::AssetsReaderState& readers,size_t limit=512)
        :readers_(readers),limit_(limit) {
        if(!limit || limit>4096) throw std::invalid_argument("Invalid character catalog limit");
    }
    bool advance(size_t candidateBudget=16) {
        if(!candidateBudget) throw std::invalid_argument("Empty character catalog batch");
        if(done_) return true;
        std::unique_lock lock(*readers_.queryMutex,std::try_to_lock);
        if(!lock.owns_lock()) return false;
        if(serialIndex_==serials_.size()) {
            serials_.clear();serialIndex_=0;
            if(version_==unsigned(CrossGateVersion::COUNT)) return finish();
            const auto version=CrossGateVersion(version_++);
            auto* infoReader=readers_.pAnimeInfoReader[version].get();
            if(!infoReader || !readers_.pAnimeDataReader[version] || !readers_.pGfxInfoReader[version]) return false;
            if(infoReader->queryAvailableSerialNums(&serials_)!=Results::Success) {serials_.clear();return false;}
            std::sort(serials_.begin(),serials_.end(),[](auto a,auto b){return a.value<b.value;});
            return false; // Enumeration and candidate reads use separate polls.
        }
        const auto version=CrossGateVersion(version_-1);
        for(size_t n=0;n<candidateBudget && serialIndex_<serials_.size() && result_.size()<limit_;++n)
            inspect(version,serials_[serialIndex_++]);
        if(result_.size()==limit_) return finish();
        return false;
    }
    size_t size() const noexcept {return result_.size();}
    std::vector<AnimationRequest> take() {
        if(!done_) throw std::logic_error("Character catalog scan is incomplete");
        return std::move(result_);
    }
private:
    bool finish() {
        if(result_.empty()) throw std::runtime_error("No character appearances in installed assets");
        done_=true;std::vector<AnimeResourceSerialNum>().swap(serials_);return true;
    }
    void inspect(CrossGateVersion version,AnimeResourceSerialNum serial) {
        auto* infoReader=readers_.pAnimeInfoReader[version].get();
        auto* dataReader=readers_.pAnimeDataReader[version].get();
        auto* gfxReader=readers_.pGfxInfoReader[version].get();
        AnimeResourceInfo info{}; AnimeResourceData data{};
        if(infoReader->query(serial,&info)!=Results::Success || dataReader->query(info,&data)!=Results::Success) return;
        bool valid=true;
        for(unsigned d=0;d<8;++d) for(unsigned m=0;m<2;++m) {
            auto it=data.motionMap.find({DirectionTypes(d),MotionTypes(m)});
            if(it==data.motionMap.end() || !it->second.duration || it->second.motionGraphicsSerialNums.empty()) valid=false;
        }
        if(!valid) return;
        const unsigned direction=serial.value%8;
        const auto& frames=data.motionMap.at({DirectionTypes(direction),MotionTypes(0)}).motionGraphicsSerialNums;
        if(frames.size()>256) return;
        int64_t left=0,top=0,right=0,bottom=0;
        for(auto graphic:frames) {
            GraphicsResourceInfo frame{};
            if(gfxReader->query({GraphicsResourceSerialNumTypes::GraphicsSerialNum,version,graphic},&frame)!=Results::Success ||
               frame.width<=0 || frame.height<=0 || frame.width>4096 || frame.height>4096) {valid=false;break;}
            {left=std::min(left,int64_t(frame.offsetX));top=std::min(top,int64_t(frame.offsetY));
                right=std::max(right,int64_t(frame.offsetX)+frame.width);bottom=std::max(bottom,int64_t(frame.offsetY)+frame.height);}
        }
        if(!valid || right-left>4096 || bottom-top>4096 || (right-left)*(bottom-top)*frames.size()*4>32ull*1024*1024) return;
        result_.push_back({serial.value,direction,0,version});
    }
    component::AssetsReaderState& readers_;
    size_t limit_,serialIndex_=0;
    unsigned version_=0;
    bool done_=false;
    std::vector<AnimeResourceSerialNum> serials_;
    std::vector<AnimationRequest> result_;
};
// Blocking convenience for offline/preloaded benchmark use; shares all filters.
inline std::vector<AnimationRequest> ReadCharacterCatalog(component::AssetsReaderState& readers,size_t limit=512) {
    CharacterCatalogScan scan(readers,limit);
    while(!scan.advance()) std::this_thread::yield();
    return scan.take();
}
}
