#include "benchmark/crowd_scenario.h"
#include "assets/assets_components.h"
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "map/map_render_data.h"
#include "engine/engine_components.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include <random>

namespace cgl {
void CrowdScenario::enterCrowd(cgl::ECSCore* world, const cgl::MapLayout& layout, const cgl::Settings& settings,
                cgl::component::AssetsReaderState& readers, cgl::graphics::IDevice* device,
                cgl::graphics::IRenderPass* pass, cgl::component::CrowdBenchmark& state) {
    const auto& b=settings.benchmark;
    if (b.ActorCount>20000 || b.ActorsPerCell>10 || !b.UniqueCharacters || b.UniqueCharacters>20000)
        throw std::runtime_error("Invalid crowd density/count/unique limit");
    const auto& window=*world->getSingleton<cgl::component::WindowHandle>();
    state.width=window.width;state.height=window.height;
    const cgl::Viewport viewport(0,0,window.width,window.height);
    const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
    const auto bounds=cgl::VisibleMapBounds(camera,viewport,layout.width,layout.height,0,0);
    std::vector<cgl::MapPoint> cells;
    for(int y=bounds.top;y<=bounds.bottom;++y) for(int x=bounds.left;x<=bounds.right;++x) {
        const cgl::MapPoint cell{double(x),double(y)};
        const auto p=cgl::MapToScreen(cell,camera,viewport);
        if(p.x>0 && p.y>0 && p.x<viewport.width && p.y<viewport.height) cells.push_back(cell);
    }
    state.visibleCells=uint32_t(cells.size());
    const size_t perCell=b.ActorsPerCell ? b.ActorsPerCell : 1;
    const size_t count=b.ActorCount ? b.ActorCount : cells.size()*perCell;
    if(cells.empty() || count>20000 || count>cells.size()*10 || b.UniqueCharacters>count)
        throw std::runtime_error("Crowd count exceeds visible cell capacity or unique count exceeds actors");
    std::vector<cgl::AnimationRequest> requests;
    auto& clips=crowdClips_; auto& walks=crowdWalks_;
    {
        std::lock_guard readerLock(*readers.queryMutex);
        for(unsigned v=0;v<unsigned(cgl::CrossGateVersion::COUNT) && requests.size()<b.UniqueCharacters;++v) {
            const auto version=cgl::CrossGateVersion(v);
            auto* infoReader=readers.pAnimeInfoReader[version].get();
            auto* dataReader=readers.pAnimeDataReader[version].get();
            if(!infoReader || !dataReader) continue;
            std::vector<cgl::AnimeResourceSerialNum> serials;
            if(infoReader->queryAvailableSerialNums(&serials)!=cgl::Results::Success) continue;
            std::sort(serials.begin(),serials.end(),[](const auto& a,const auto& b){return a.value<b.value;});
            for(const auto& serial:serials) {
                cgl::AnimeResourceInfo info{}; cgl::AnimeResourceData data{};
                if(infoReader->query(serial,&info)!=cgl::Results::Success ||
                   dataReader->query(info,&data)!=cgl::Results::Success) continue;
                if (!cgl::IsCrowdCharacterDescription(data)) { ++state.filtered; continue; }
                const auto key=cgl::AnimeDataMapKey{cgl::DirectionTypes(0),cgl::MotionTypes(0)};
                auto motion=data.motionMap.find(key);
                if(motion==data.motionMap.end() || motion->second.motionGraphicsSerialNums.empty() || !motion->second.duration) continue;
                bool validMetadata = true;
                for (unsigned m=0;m<2;++m) {
                    const auto& frameIds=data.motionMap.at({cgl::DirectionTypes(0),cgl::MotionTypes(m)}).motionGraphicsSerialNums;
                    for (auto graphic:frameIds) {
                        cgl::GraphicsResourceInfo frame{};
                        if (!readers.pGfxInfoReader[version] || readers.pGfxInfoReader[version]->query(
                            {cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum,version,graphic},&frame)!=cgl::Results::Success ||
                            frame.width<=0 || frame.height<=0 || frame.width>4096 || frame.height>4096) {
                            validMetadata=false; break;
                        }
                    }
                    if(!validMetadata) break;
                }
                if(!validMetadata) { ++state.filtered; continue; }
                requests.push_back({serial.value,0,0,version});
                if(requests.size()==b.UniqueCharacters) break;
            }
        }
    }
    if(requests.size()!=b.UniqueCharacters) throw std::runtime_error("Insufficient distinct animation resources in installed corpus");
    // Metadata is selected at entry; pixel decoding/upload for cold resources
    // starts inside the measured loop, either synchronously or via the CPU worker.
    if(b.LoadDuringPlayback) {
        std::mt19937 random(0xC61);
        std::shuffle(requests.begin(),requests.end(),random);
    }
    for(const auto& r:requests) state.resources.emplace_back(unsigned(r.version),r.serial);
    const size_t initial=b.LoadDuringPlayback ? 1 : requests.size();
    for(size_t i=0;i<initial;++i) loadCrowdClip(requests[i],readers,device,pass,b.SwitchMotions,state);
    coldRequests_=std::move(requests);
    for(size_t i=0;i<count;++i) {
        const auto cellIndex=i%cells.size();
        const auto entity=world->createEntity();
        try {crowd_.push_back(entity);} catch(...) {world->destroyEntity(entity);throw;}
        world->addComponent<cgl::component::MapPosition>(entity,cells[cellIndex]);
        const auto resource=cgl::CrowdResourceIndex(i,cells.size(),coldRequests_.size());
        world->addComponent<cgl::component::Sprite>(entity,clips[resource<clips.size()?resource:0]);
        const auto ready=resource<clips.size()?resource:0;
        world->addComponent<cgl::component::CrowdActor>(entity,clips[ready],b.SwitchMotions?walks[ready]:nullptr,resource);
        world->addComponent<cgl::component::AnimationPlayer>(entity,double((i*73)%1000));
        ++state.actors;
    }
    LOGI("Crowd ready: cells=" << cells.size() << ", actors=" << count << ", resources=" << clips.size());
}
void CrowdScenario::loadCrowdClip(const cgl::AnimationRequest& request,cgl::component::AssetsReaderState& readers,
    cgl::graphics::IDevice* device,cgl::graphics::IRenderPass* pass,bool switchMotions,
    cgl::component::CrowdBenchmark& state) {
    auto idle=animations_.acquire(request,readers,device,pass);
    auto walk=switchMotions ? animations_.acquire({request.serial,request.direction,1,request.version},readers,device,pass) : nullptr;
    storeCrowdClip(std::move(idle),std::move(walk),state);
}
void CrowdScenario::storeCrowdClip(std::shared_ptr<const cgl::AnimationClip> idle,std::shared_ptr<const cgl::AnimationClip> walk,
                    cgl::component::CrowdBenchmark& state) {
    state.animationBytes+=uint64_t(idle->width)*idle->height*idle->frameCount*4;
    if(walk) state.animationBytes+=uint64_t(walk->width)*walk->height*walk->frameCount*4;
    crowdClips_.push_back(std::move(idle)); crowdWalks_.push_back(std::move(walk));
    state.unique=uint32_t(crowdClips_.size());
}
void CrowdScenario::loadColdCharacter(cgl::ECSCore* world,cgl::component::CrowdBenchmark& state) {
    const auto& b=world->getSingleton<cgl::component::EngineState>()->settings->benchmark;
    // Scene entry also updates once, before benchmark sampling begins.
    if(!b.LoadDuringPlayback || crowdFrames_++<=b.WarmupFrames || state.unique>=coldRequests_.size()) return;
    const auto start=std::chrono::steady_clock::now();
    const size_t resource=state.unique;
    auto& readers=*world->getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world->getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    if(coldStart_==std::chrono::steady_clock::time_point{}) coldStart_=start;
    if(b.ProgressiveUploads) {
        const auto& request=coldRequests_[resource];
        auto idle=animations_.acquireProgressive(request,readers,device,pass,b.BackgroundDecode);
        auto walk=idle && b.SwitchMotions ? animations_.acquireProgressive({request.serial,request.direction,1,request.version},readers,device,pass,b.BackgroundDecode) : nullptr;
        if(!idle || (b.SwitchMotions && !walk)) {
            coldWorkMs_+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            return;
        }
        storeCrowdClip(std::move(idle),std::move(walk),state);
    } else loadCrowdClip(coldRequests_[resource],readers,device,pass,b.SwitchMotions,state);
    world->forEach<cgl::component::CrowdActor,cgl::component::Sprite,cgl::component::AnimationPlayer>(
        [&](auto,auto* actor,auto* sprite,auto* player) {
            if(actor->resource!=resource) return;
            actor->idle=crowdClips_[resource]; actor->walk=crowdWalks_[resource];
            sprite->clip=crowdWalking_?actor->walk:actor->idle;
            player->timeMs=0;player->frame=0;
        });
    const auto end=std::chrono::steady_clock::now();
    state.coldLoadMs.push_back(coldWorkMs_+std::chrono::duration<double,std::milli>(end-start).count());
    state.coldReadyMs.push_back(std::chrono::duration<double,std::milli>(end-coldStart_).count());
    coldWorkMs_=0;coldStart_={};
}
void CrowdScenario::enter(ECSCore* world,const MapLayout& layout,component::CrowdBenchmark& state) {
    const auto& settings=*world->getSingleton<component::EngineState>()->settings;
    auto& readers=*world->getSingleton<component::AssetsReaderState>();
    auto* device=world->getSingleton<component::RenderDeviceState>()->pDevice.get();
    auto* pass=world->getSingleton<component::PrimarySceneRenderFrame>()->renderPasses.at(PRIMARY_RENDER_PASS).get();
    enterCrowd(world,layout,settings,readers,device,pass,state);
}
void CrowdScenario::update(ECSCore* world,component::CrowdBenchmark& state) {
    loadColdCharacter(world,state);
    const auto seconds=world->getSingleton<component::FrameTime>()->deltaSeconds;
    auto* benchmark=&state;
    crowdSeconds_ += seconds;
    const bool walking=(uint64_t(crowdSeconds_)%2)!=0;
    if (world->getSingleton<cgl::component::EngineState>()->settings->benchmark.SwitchMotions && walking!=crowdWalking_) {
        world->forEach<cgl::component::CrowdActor,cgl::component::Sprite,cgl::component::AnimationPlayer>(
            [&](auto,auto* actor,auto* sprite,auto* player){ sprite->clip=walking?actor->walk:actor->idle;player->timeMs=0;player->frame=0; });
        crowdWalking_=walking; ++benchmark->switches;
    }
    cgl::UpdateAnimations(*world,seconds);
}
void CrowdScenario::clear(ECSCore* world) {
    for(auto entity:crowd_) world->destroyEntity(entity);
    crowd_.clear();crowdClips_.clear();crowdWalks_.clear();coldRequests_.clear();
    coldWorkMs_=0;coldStart_={};crowdFrames_=0;crowdSeconds_=0;crowdWalking_=false;
}
}
