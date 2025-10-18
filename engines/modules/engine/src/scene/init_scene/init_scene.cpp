// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <assert.h>
#include <memory>
#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "render/render_components.h"
#include "assets/assets_system.h"
#include "scene/scene_components.h"
#include "scene/init_scene/init_scene_systems.h"

using cgl::component::AssetsReaderState;

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
    cgl::AssetsReaderInitSystem assetsReaderInitSys_;
    cgl::component::AssetsReaderState* pAssetsReaderState_;
};

}   // namespace


// -----------------------------------------------------------------------------
InitScene::InitScene()
    : cgl::IScene(cgl::SceneTypes::InitScene) {
}

void InitScene::onEnter(cgl::ECSCore* pECS) noexcept {
    pAssetsReaderState_ = pECS->getSingleton<AssetsReaderState>();
    assert(pAssetsReaderState_ != nullptr);

    cgl::InitSceneInitRenderSystem initRenderSys;
    initRenderSys.update(pECS);
    assetsReaderInitSys_.update(pECS);
}

void InitScene::onExit(cgl::ECSCore* pECS) noexcept {
    cgl::InitSceneDestroySystem destroySys;
    destroySys.update(pECS);
}

void InitScene::update(cgl::ECSCore* pECS) noexcept {
    // render
    renderSys_.update(pECS);

    // check resource init finish
    assetsReaderInitSys_.update(pECS);
}

// -----------------------------------------------------------------------------
cgl::IScene::Ptr cgl::CreateInitScene() {
    return std::make_unique<InitScene>();
}
