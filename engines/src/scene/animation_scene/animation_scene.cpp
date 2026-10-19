#include "scene/scene_components.h"
#include "scene/animation_scene/character_animation.h"
#include "assets/assets_components.h"
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "engine/error_system.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"

namespace {
class AnimationScene final : public cgl::IScene {
public:
    AnimationScene() : IScene(cgl::SceneTypes::AnimationScene) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        try {
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(
                cgl::PRIMARY_RENDER_PASS).get();
            animation_.load(*world->getSingleton<cgl::component::EngineState>()->settings,
                *world->getSingleton<cgl::component::AssetsReaderState>(), render.pDevice.get(), pass);
            LOGI("Animation scene entered");
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(),
                "Character animation failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        // This scene owns the texture/pipeline. Pending draws must finish first.
        auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
        if (render.pDevice) render.pDevice->waitIdle();
        animation_.reset();
        LOGI("Animation scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::AnimationScene;
        }
    }
    void draw(cgl::ECSCore*, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        animation_.draw(command, viewport);
    }
private:
    cgl::CharacterAnimation animation_;
};
}
cgl::IScene::Ptr cgl::CreateAnimationScene() { return std::make_unique<AnimationScene>(); }
