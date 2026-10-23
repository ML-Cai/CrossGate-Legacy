#pragma once
#include "assets/animation_builder.h"
#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include <stdexcept>
namespace cgl {
struct AnimationClip;
// Bounded scene-entry plan. Metadata admission precedes acquisition; partial
// ownership stays here until publication or reset/onExit. No ECS/window/GPU calls.
class SceneAnimationLoad {
public:
    static constexpr size_t MaxClips=16; // Eight directions, idle and walk.
    template<class Planner> void begin(std::vector<AnimationRequest> requests,uint64_t budget,Planner&& planner) {
        reset();
        if(requests.empty() || requests.size()>MaxClips || !budget)
            throw std::invalid_argument("Invalid scene animation plan");
        uint64_t bytes=0;
        for(auto it=requests.begin();it!=requests.end();++it) {
            if(std::find(requests.begin(),it,*it)!=it) throw std::invalid_argument("Duplicate scene animation key");
            const auto required=planner(*it);
            if(!required || required>budget-bytes) throw std::runtime_error("Scene animation working set exceeds total texture budget");
            bytes+=required;
        }
        requests_=std::move(requests);requiredBytes_=bytes;
    }
    template<class Acquire> bool advance(Acquire&& acquire) {
        while(completed_<requests_.size()) {
            auto clip=acquire(requests_[completed_]);
            if(!clip) return false;
            clips_[completed_++]=std::move(clip);
        }
        return ready();
    }
    bool ready() const noexcept {return !requests_.empty() && completed_==requests_.size();}
    size_t completed() const noexcept {return completed_;}
    size_t total() const noexcept {return requests_.size();}
    uint64_t requiredBytes() const noexcept {return requiredBytes_;}
    const std::shared_ptr<const AnimationClip>& clip(size_t index) const {
        if(index>=completed_) throw std::out_of_range("Scene clip is not ready");
        return clips_[index];
    }
    void reset() noexcept {clips_.fill(nullptr);requests_.clear();completed_=0;requiredBytes_=0;}
private:
    std::vector<AnimationRequest> requests_;
    std::array<std::shared_ptr<const AnimationClip>,MaxClips> clips_{};
    size_t completed_=0;
    uint64_t requiredBytes_=0;
};
}
