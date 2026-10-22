#pragma once
#include "assets/animation_library.h"
#include "cgl/assets/map_layout_reader.h"
#include "benchmark/crowd_benchmark.h"
#include "engine/ecs.h"
#include <chrono>

namespace cgl {
// Owns only benchmark entities and playback state. The scene must call clear()
// before destruction, including after partial entry. GPU clips stay engine-owned.
class CrowdScenario {
public:
    explicit CrowdScenario(AnimationLibrary& animations):animations_(animations) {}
    CrowdScenario(const CrowdScenario&)=delete;
    CrowdScenario& operator=(const CrowdScenario&)=delete;
    void enter(ECSCore* world,const MapLayout& layout,component::CrowdBenchmark& state);
    void update(ECSCore* world,component::CrowdBenchmark& state);
    void clear(ECSCore* world);
private:
    void enterCrowd(cgl::ECSCore* world, const cgl::MapLayout& layout, const cgl::Settings& settings,
                    cgl::component::AssetsReaderState& readers, cgl::graphics::IDevice* device,
                    cgl::graphics::IRenderPass* pass, cgl::component::CrowdBenchmark& state);
    void loadCrowdClip(const cgl::AnimationRequest& request,cgl::component::AssetsReaderState& readers,
        cgl::graphics::IDevice* device,cgl::graphics::IRenderPass* pass,bool switchMotions,
        cgl::component::CrowdBenchmark& state);
    void storeCrowdClip(std::shared_ptr<const cgl::AnimationClip> idle,std::shared_ptr<const cgl::AnimationClip> walk,
                        cgl::component::CrowdBenchmark& state);
    void loadColdCharacter(cgl::ECSCore* world,cgl::component::CrowdBenchmark& state);
    AnimationLibrary& animations_;
    std::vector<cgl::Entity> crowd_;
    std::vector<std::shared_ptr<const cgl::AnimationClip>> crowdClips_,crowdWalks_;
    std::vector<cgl::AnimationRequest> coldRequests_;
    double coldWorkMs_=0;
    std::chrono::steady_clock::time_point coldStart_{};
    uint32_t crowdFrames_=0;
    double crowdSeconds_=0;
    bool crowdWalking_=false;
};
}
