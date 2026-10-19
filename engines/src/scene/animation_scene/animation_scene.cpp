#include "scene/scene_components.h"
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
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(
                cgl::PRIMARY_RENDER_PASS).get();
            auto clip = library_.acquire({settings.animation.SerialNum, settings.animation.Direction, settings.animation.Motion},
                *world->getSingleton<cgl::component::AssetsReaderState>(), render.pDevice.get(), pass);
            auto* window = world->getSingleton<cgl::component::WindowHandle>();
            entities_.reserve(2);
            for (int i = 0; i < 2; ++i) {
                const auto entity = world->createEntity();
                entities_.push_back(entity); // Include partial entries in onExit cleanup.
                world->addComponent<cgl::component::Transform2D>(entity,
                    window->width * (i ? 0.75f : 0.25f) - clip->width * scale / 2,
                    (window->height - clip->height * scale) / 2, scale);
                world->addComponent<cgl::component::Sprite>(entity, clip);
                world->addComponent<cgl::component::AnimationPlayer>(entity,
                    i ? clip->durationMs / 2.0 : 0.0, 1.0);
            }
            cgl::UpdateAnimations(*world, 0);
            LOGI("Animation scene entered");
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(),
                "Character animation failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        for (auto entity : entities_) world->destroyEntity(entity);
        entities_.clear();
        // The engine cache retains clips used by pending GPU submissions.
        LOGI("Animation scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        cgl::UpdateAnimations(*world, world->getSingleton<cgl::component::FrameTime>()->deltaSeconds);
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::AnimationScene;
        }
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        world->forEach<cgl::component::Transform2D, cgl::component::Sprite, cgl::component::AnimationPlayer>(
            [&](cgl::Entity, auto* transform, auto* sprite, auto* player) {
                if (sprite->clip) sprite->clip->renderer->draw(command, viewport, player->frame,
                    transform->x, transform->y, transform->scale);
            });
    }
private:
    cgl::AnimationLibrary& library_;
    std::vector<cgl::Entity> entities_;
};
}
cgl::IScene::Ptr cgl::CreateAnimationScene(AnimationLibrary& library) {
    return std::make_unique<AnimationScene>(library);
}
