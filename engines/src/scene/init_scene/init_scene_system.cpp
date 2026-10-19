#include "scene/scene_components.h"
#include "assets/assets_components.h"
#include "assets/assets_system.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "cgl/trace/logger.h"

namespace {
class InitScene final : public cgl::IScene {
public:
    InitScene() : IScene(cgl::SceneTypes::InitScene) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        LOGI("Init scene entered");
        if (world->getSingleton<cgl::component::AssetsReaderState>()->state != cgl::StateTypes::RUNNING)
            loader_.update(world);
    }
    void onExit(cgl::ECSCore*) noexcept override { LOGI("Init scene exited"); }
    void update(cgl::ECSCore* world) noexcept override {
        auto* assets = world->getSingleton<cgl::component::AssetsReaderState>();
        if (assets->state != cgl::StateTypes::RUNNING) loader_.update(world);
        auto* scene = world->getSingleton<cgl::component::SceneState>();
        if (assets->state == cgl::StateTypes::ERROR) cgl::RaiseError(scene, assets->lastError);
        else if (assets->state == cgl::StateTypes::RUNNING) scene->nextScene = cgl::SceneTypes::AnimationScene;
    }
private:
    cgl::AssetsReaderInitSystem loader_;
};
}
cgl::IScene::Ptr cgl::CreateInitScene() { return std::make_unique<InitScene>(); }
