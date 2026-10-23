#include "scene/scene_components.h"
#include "scene/scene_animation_load.h"
#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "map/map_render_data.h"
#include "map/map_hover.h"
#include "map/map_world_update.h"
#include "map/map_world_renderer.h"
#include "map/server_demo_scenario.h"
#include "benchmark/crowd_scenario.h"
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "engine/error_system.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include <stdexcept>
#include <chrono>
#include "telemetry/frame_telemetry.h"
#include "benchmark/crowd_benchmark.h"

namespace {
class MapScene final : public cgl::IScene {
public:
    MapScene(cgl::MapLibrary& maps, cgl::AnimationLibrary& animations)
        : IScene(cgl::SceneTypes::MapScene), maps_(maps), animations_(animations) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        const auto loadStart = std::chrono::steady_clock::now();
        if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>()) *b={};
        try {
            const auto& settings = *world->getSingleton<cgl::component::EngineState>()->settings;
            const auto scale = settings.display.ViewportScale;
            if (!std::isfinite(scale) || scale > 16 || scale < 0.125)
                throw std::runtime_error("Map ViewportScale must be between 0.125 and 16");
            if(!world->getSingleton<cgl::component::CrowdBenchmark>()) {
                if(settings.animation.Direction>=8) throw std::runtime_error("Invalid initial character direction");
                facing_=settings.animation.Direction;serverDemoEnabled_=settings.serverdemo.Enabled;
                std::vector<cgl::AnimationRequest> requests;
                for(uint32_t direction=0;direction<8;++direction) {
                    requests.push_back({settings.animation.SerialNum,direction,0});
                    requests.push_back({settings.animation.SerialNum,direction,1});
                }
                cgl::AnimationBuilder planner;
                load_.begin(std::move(requests),animations_.usage().budgetBytes,[&](const auto& request) {
                    return planner.requiredBytes(request,*world->getSingleton<cgl::component::AssetsReaderState>());
                });
                world->addSingleton<cgl::component::SceneLoadProgress>(0,uint32_t(load_.total()),load_.requiredBytes(),false);
            }
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
            auto& readers = *world->getSingleton<cgl::component::AssetsReaderState>();
            map_ = maps_.acquire(std::filesystem::path(settings.general.EngineRootPath) / settings.map.FilePath,
                readers, render.pDevice.get(), pass);
            const cgl::MapPoint center{double(map_->layout.width / 2), double(map_->layout.height / 2)};
            world->addSingleton<cgl::component::MapCamera>(center, scale);
            world->addSingleton<cgl::component::MapHover>();
            outline_ = maps_.acquireHoverOutline(scale, render.pDevice.get(), pass);
            if (auto* benchmark = world->getSingleton<cgl::component::CrowdBenchmark>()) {
                crowdScenario_=std::make_unique<cgl::CrowdScenario>(animations_);
                crowdScenario_->enter(world,map_->layout,*benchmark);
                benchmark->loadMs = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-loadStart).count();
                benchmark->ready = true;ready_=true;
                cgl::UpdateAnimations(*world,0);
                return;
            }
            advanceEntry(world);
        } catch (const std::exception& error) {
            if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>())
                b->loadMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-loadStart).count();
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map scene failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>()) b->ready=false;
        if(crowdScenario_) crowdScenario_->clear(world);
        if(serverDemo_) serverDemo_->clear(world);
        crowdScenario_.reset();serverDemo_.reset();
        animations_.cancelPending();
        if (character_) world->destroyEntity(character_);
        character_ = 0;ready_=false;load_.reset();
        world->removeSingleton<cgl::component::SceneLoadProgress>();
        world->removeSingleton<cgl::component::MapCamera>();
        world->removeSingleton<cgl::component::MapHover>();
        outline_.reset();
        map_.reset(); // Engine caches retain resources for pending submissions.
        renderer_.clear();
        LOGI("Map scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        if(world->getSingleton<cgl::component::SceneState>()->state==cgl::StateTypes::ERROR) return;
        if(!ready_) {
            auto* input=world->getSingleton<cgl::component::KeyboardInputState>();
            if(input->restartSceneRequested) {
                input->restartSceneRequested=false;
                world->getSingleton<cgl::component::SceneState>()->nextScene=cgl::SceneTypes::MapScene;return;
            }
            try {advanceEntry(world);} catch(const std::exception& error) {
                cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(),"Map scene failed: ",error.what());
            }
            return;
        }
        const auto seconds = world->getSingleton<cgl::component::FrameTime>()->deltaSeconds;
        if (auto* benchmark=world->getSingleton<cgl::component::CrowdBenchmark>()) {
            world->getSingleton<cgl::component::MapHover>()->cell.reset();
            try { crowdScenario_->update(world,*benchmark); }
            catch (const std::exception& error) {
                cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Cold character load failed: ",error.what());
            }
            return;
        }
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        cgl::UpdateMapWorld(*world,character_,map_->layout.width,map_->layout.height,
            input->moveX,input->moveY,seconds);
        const auto& window = *world->getSingleton<cgl::component::WindowHandle>();
        const cgl::Viewport viewport(0,0,window.width,window.height);
        if(serverDemo_) {
            try {serverDemo_->update(world,viewport);}
            catch(const std::exception& error) {
                cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Server demo failed: ",error.what());
            }
        }
        updateHover(world, viewport);
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::MapScene;
        }
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        if(!ready_) return;
        try {
            auto* benchmark=world->getSingleton<cgl::component::CrowdBenchmark>();
            const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
            const auto hovered=benchmark ? std::optional<cgl::MapCellCoord>{} :
                cgl::ResolveMapHover(pointerInput(world),camera,viewport,map_->layout);
            const auto stats=renderer_.draw(*world,*map_,camera,hovered,outline_.get(),command,viewport);
            if(auto* telemetry=world->getSingleton<cgl::component::RuntimeTelemetry>())
                telemetry->rendering={0,0,0,stats.sortMs,stats.drawnActors,stats.drawCalls};
            if(benchmark) {
                benchmark->drawnActors=stats.drawnActors;
                benchmark->drawCalls=stats.drawCalls;
                benchmark->sortMs=stats.sortMs;
            }
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map drawing failed: ", error.what());
        }
    }
private:
    void advanceEntry(cgl::ECSCore* world) {
        auto& render=*world->getSingleton<cgl::component::RenderDeviceState>();
        auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
        const bool acquired=load_.advance([&](const auto& request) {
            return animations_.acquireForScene(request,*world->getSingleton<cgl::component::AssetsReaderState>(),render.pDevice.get(),pass);
        });
        if(acquired) {
            cgl::component::CharacterMovement movement;movement.facing=facing_;
            for(size_t direction=0;direction<8;++direction) {
                movement.idle[direction]=load_.clip(direction*2);movement.walk[direction]=load_.clip(direction*2+1);
            }
            const auto center=world->getSingleton<cgl::component::MapCamera>()->center;
            character_=world->createEntity();
            world->addComponent<cgl::component::MapPosition>(character_,center);
            world->addComponent<cgl::component::Sprite>(character_,movement.idle[facing_]);
            world->addComponent<cgl::component::AnimationPlayer>(character_);
            world->addComponent<cgl::component::CharacterMovement>(character_,std::move(movement));
            if(serverDemoEnabled_) {
                serverDemo_=std::make_unique<cgl::ServerDemoScenario>(animations_);serverDemo_->enter(world,map_->layout,character_);
            }
            cgl::UpdateAnimations(*world,0);ready_=true;
            world->getSingleton<cgl::component::WindowHandle>()->pendingTitle="CGL";
            LOGI("Map scene entered");
        } else if(lastProgress_!=load_.completed()) {
            world->getSingleton<cgl::component::WindowHandle>()->pendingTitle="CGL | Loading "+
                std::to_string(load_.completed())+"/"+std::to_string(load_.total());
            lastProgress_=load_.completed();
        }
        *world->getSingleton<cgl::component::SceneLoadProgress>()={uint32_t(load_.completed()),uint32_t(load_.total()),load_.requiredBytes(),ready_};
    }
    static cgl::MapPointerInput pointerInput(cgl::ECSCore* world) noexcept {
        const auto* cursor=world->getSingleton<cgl::component::CursorPositionState>();
        if(!cursor) return {};
        const auto& window=*world->getSingleton<cgl::component::WindowHandle>();
        return {{cursor->x,cursor->y},{double(cursor->windowWidth),double(cursor->windowHeight)},
            {double(window.width),double(window.height)},cursor->inside};
    }
    void updateHover(cgl::ECSCore* world,const cgl::Viewport& viewport) noexcept {
        world->getSingleton<cgl::component::MapHover>()->cell=cgl::ResolveMapHover(pointerInput(world),
            *world->getSingleton<cgl::component::MapCamera>(),viewport,map_->layout);
    }
    cgl::MapLibrary& maps_;
    cgl::AnimationLibrary& animations_;
    std::shared_ptr<const cgl::MapResources> map_;
    std::shared_ptr<const cgl::MapHoverOutline> outline_;
    cgl::SceneAnimationLoad load_;
    size_t lastProgress_=std::numeric_limits<size_t>::max();
    uint32_t facing_=0;
    bool ready_=false,serverDemoEnabled_=false;
    cgl::Entity character_ = 0;
    std::unique_ptr<cgl::CrowdScenario> crowdScenario_;
    std::unique_ptr<cgl::ServerDemoScenario> serverDemo_;
    cgl::MapWorldRenderer renderer_;
};
}
cgl::IScene::Ptr cgl::CreateMapScene(MapLibrary& maps, AnimationLibrary& animations) {
    return std::make_unique<MapScene>(maps, animations);
}
