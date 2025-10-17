// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "assert.h"
#include <memory>
#include "cgl/trace/logger.h"
#include "render/render_components.h"
#include "scene/scene_components.h"
#include "engine/ecs.h"
#include "scene/init_scene/init_scene_systems.h"

// -----------------------------------------------------------------------------
namespace {

class InitScene : public cgl::IScene {
 public:
    InitScene();

    ~InitScene() = default;

    void onEnter(cgl::ECSCore* pECS) noexcept override;

    void onExit(cgl::ECSCore* pECS) noexcept override;

    void update(cgl::ECSCore* pECS) noexcept override;

 private:
    cgl::InitSceneRenderSystem renderSys_;
};

}   // namespace


// -----------------------------------------------------------------------------
InitScene::InitScene()
    : cgl::IScene(cgl::SceneTypes::InitScene) {
}

void InitScene::onEnter(cgl::ECSCore* pECS) noexcept {
    cgl::InitSceneInitRenderSystem initRenderSys;
    initRenderSys.update(pECS);
}

void InitScene::onExit(cgl::ECSCore* pECS) noexcept {
    cgl::InitSceneDestroySystem destroySys;
    destroySys.update(pECS);
}

void InitScene::update(cgl::ECSCore* pECS) noexcept {
    // render
    renderSys_.update(pECS);

    // check resource init finish
    
}

// -----------------------------------------------------------------------------
cgl::IScene::Ptr cgl::CreateInitScene() {
    return std::make_unique<InitScene>();
}
