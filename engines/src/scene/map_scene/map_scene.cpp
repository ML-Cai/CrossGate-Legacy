#include "scene/scene_components.h"
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
                benchmark->ready = true;
                cgl::UpdateAnimations(*world,0);
                return;
            }
            if (settings.animation.Direction >= 8)
                throw std::runtime_error("Invalid initial character direction");
            cgl::component::CharacterMovement movement;
            movement.facing = settings.animation.Direction;
            // Load before entering the frame loop: turning never uploads GPU resources.
            for (uint32_t direction = 0; direction < 8; ++direction) {
                movement.idle[direction] = animations_.acquire({settings.animation.SerialNum, direction, 0},
                    readers, render.pDevice.get(), pass);
                movement.walk[direction] = animations_.acquire({settings.animation.SerialNum, direction, 1},
                    readers, render.pDevice.get(), pass);
            }
            auto clip = movement.idle[movement.facing];
            character_ = world->createEntity();
            world->addComponent<cgl::component::MapPosition>(character_, center);
            world->addComponent<cgl::component::Sprite>(character_, std::move(clip));
            world->addComponent<cgl::component::AnimationPlayer>(character_);
            world->addComponent<cgl::component::CharacterMovement>(character_, std::move(movement));
            if(settings.serverdemo.Enabled) {
                serverDemo_=std::make_unique<cgl::ServerDemoScenario>(animations_);
                serverDemo_->enter(world,map_->layout,character_);
            }
            cgl::UpdateAnimations(*world, 0);
            LOGI("Map scene entered");
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
        character_ = 0;
        world->removeSingleton<cgl::component::MapCamera>();
        world->removeSingleton<cgl::component::MapHover>();
        outline_.reset();
        map_.reset(); // Engine caches retain resources for pending submissions.
        renderer_.clear();
        LOGI("Map scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
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
        try {
            auto* benchmark=world->getSingleton<cgl::component::CrowdBenchmark>();
            const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
            const auto hovered=benchmark ? std::optional<cgl::MapCellCoord>{} :
                cgl::ResolveMapHover(pointerInput(world),camera,viewport,map_->layout);
            const auto stats=renderer_.draw(*world,*map_,camera,hovered,outline_.get(),command,viewport);
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
    cgl::Entity character_ = 0;
    std::unique_ptr<cgl::CrowdScenario> crowdScenario_;
    std::unique_ptr<cgl::ServerDemoScenario> serverDemo_;
    cgl::MapWorldRenderer renderer_;
};
}
cgl::IScene::Ptr cgl::CreateMapScene(MapLibrary& maps, AnimationLibrary& animations) {
    return std::make_unique<MapScene>(maps, animations);
}
