#include "scene/scene_components.h"
#include "scene/scene_animation_load.h"
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "assets/assets_components.h"
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "engine/error_system.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
class AnimationScene final : public cgl::IScene {
public:
    explicit AnimationScene(cgl::AnimationLibrary& library)
        : IScene(cgl::SceneTypes::AnimationScene), library_(library) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        try {
            const auto& settings = *world->getSingleton<cgl::component::EngineState>()->settings;
            const auto scale = settings.display.ViewportScale;
            if (!std::isfinite(scale) || scale <= 0)
                throw std::runtime_error("ViewportScale must be positive and finite");
            scale_=scale;
            cgl::AnimationBuilder planner;
            load_.begin({{settings.animation.SerialNum,settings.animation.Direction,settings.animation.Motion}},library_.usage().budgetBytes,
                [&](const auto& request){return planner.requiredBytes(request,*world->getSingleton<cgl::component::AssetsReaderState>());});
            world->addSingleton<cgl::component::SceneLoadProgress>(0,uint32_t(load_.total()),load_.requiredBytes(),false);
            advanceEntry(world);
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(),
                "Character animation failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        for (auto entity : entities_) world->destroyEntity(entity);
        entities_.clear();load_.reset();ready_=false;
        library_.cancelPending();world->removeSingleton<cgl::component::SceneLoadProgress>();
        // The engine cache retains clips used by pending GPU submissions.
        LOGI("Animation scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        if(world->getSingleton<cgl::component::SceneState>()->state==cgl::StateTypes::ERROR) return;
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::AnimationScene;return;
        }
        if(!ready_) {
            try {advanceEntry(world);} catch(const std::exception& error) {
                cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(),"Character animation failed: ",error.what());
            }
            return;
        }
        cgl::UpdateAnimations(*world,world->getSingleton<cgl::component::FrameTime>()->deltaSeconds);
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        if(!ready_) return;
        world->forEach<cgl::component::Transform2D, cgl::component::Sprite, cgl::component::AnimationPlayer>(
            [&](cgl::Entity, auto* transform, auto* sprite, auto* player) {
                if (sprite->clip) sprite->clip->renderer->draw(command, viewport, player->frame,
                    transform->x, transform->y, transform->scale);
            });
    }
private:
    void advanceEntry(cgl::ECSCore* world) {
        auto& render=*world->getSingleton<cgl::component::RenderDeviceState>();
        auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
        if(load_.advance([&](const auto& request) {
            return library_.acquireForScene(request,*world->getSingleton<cgl::component::AssetsReaderState>(),render.pDevice.get(),pass);
        })) {
            const auto& clip=load_.clip(0);auto* window=world->getSingleton<cgl::component::WindowHandle>();
            entities_.reserve(2);
            for(int i=0;i<2;++i) {
                const auto entity=world->createEntity();entities_.push_back(entity);
                world->addComponent<cgl::component::Transform2D>(entity,window->width*(i?0.75f:0.25f)-clip->width*scale_/2,
                    (window->height-clip->height*scale_)/2,scale_);
                world->addComponent<cgl::component::Sprite>(entity,clip);
                world->addComponent<cgl::component::AnimationPlayer>(entity,i?clip->durationMs/2.0:0.0,1.0);
            }
            cgl::UpdateAnimations(*world,0);ready_=true;window->pendingTitle="CGL";
            LOGI("Animation scene entered");
        } else if(lastProgress_!=load_.completed()) {
            world->getSingleton<cgl::component::WindowHandle>()->pendingTitle="CGL | Loading "+
                std::to_string(load_.completed())+"/"+std::to_string(load_.total());lastProgress_=load_.completed();
        }
        *world->getSingleton<cgl::component::SceneLoadProgress>()={uint32_t(load_.completed()),uint32_t(load_.total()),load_.requiredBytes(),ready_};
    }
    cgl::SceneAnimationLoad load_;
    double scale_=1;
    size_t lastProgress_=std::numeric_limits<size_t>::max();
    bool ready_=false;
    cgl::AnimationLibrary& library_;
    std::vector<cgl::Entity> entities_;
};
}
cgl::IScene::Ptr cgl::CreateAnimationScene(AnimationLibrary& library) {
    return std::make_unique<AnimationScene>(library);
}
