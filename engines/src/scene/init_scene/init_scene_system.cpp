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
#include "engine/error_system.h"
#include "engine/state_system.h"
#include "render/render_components.h"
#include "assets/assets_system.h"
#include "scene/scene_components.h"
#include "scene/init_scene/init_scene_systems.h"

using cgl::component::AssetsReaderState;
using cgl::component::SceneState;
using cgl::component::RenderDeviceState;
using cgl::component::SceneRenderState;

// -----------------------------------------------------------------------------
namespace {

class InitSceneSystem : public cgl::IScene {
 public:
    InitSceneSystem();

    ~InitSceneSystem() = default;

    void onEnter(cgl::ECSCore* pECS) noexcept override;

    void onExit(cgl::ECSCore* pECS) noexcept override;

    void update(cgl::ECSCore* pECS) noexcept override;

 private:
    cgl::InitSceneRenderDrawSystem renderDrawSys_;
    cgl::AssetsReaderInitSystem assetsReaderInitSys_;

    cgl::component::SceneState* pSceneState_;
    cgl::component::RenderDeviceState* pRenderDevState_;
    cgl::component::SceneRenderState* pSceneRenderState_;
    cgl::component::AssetsReaderState* pAssetsReaderState_;
};

}   // namespace


// -----------------------------------------------------------------------------
InitSceneSystem::InitSceneSystem()
    : cgl::IScene(cgl::SceneTypes::InitScene) {
}

// -----------------------------------------------------------------------------
void InitSceneSystem::onEnter(cgl::ECSCore* pECS) noexcept {
    pSceneState_        = pECS->getSingleton<SceneState>();
    pRenderDevState_    = pECS->getSingleton<RenderDeviceState>();
    pAssetsReaderState_ = pECS->getSingleton<AssetsReaderState>();
    assert(pSceneState_ != nullptr);
    assert(pRenderDevState_ != nullptr);
    assert(pAssetsReaderState_ != nullptr);

    // inset scene render state
    pECS->addSingleton<SceneRenderState>(cgl::StateTypes::INITIALIZING, "");
    pSceneRenderState_  = pECS->getSingleton<SceneRenderState>();
    assert(pSceneRenderState_ != nullptr);

    // init render
    cgl::InitSceneRenderInitSystem renderInitSys;
    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &renderInitSys,
            .pEcs        = pECS,
            .pCheckState = pSceneRenderState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pSceneRenderState_,
                .to     = cgl::StateTypes::RUNNING
            },
            .pOnErrorState = pSceneState_})) {
        return;
    }

    // init assets readers
    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &assetsReaderInitSys_,
            .pEcs        = pECS,
            .pCheckState = pAssetsReaderState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pAssetsReaderState_,
                .to     = cgl::StateTypes::RUNNING
            },
            .pOnErrorState = pSceneState_})) {
        return;
    }
}

// -----------------------------------------------------------------------------
void InitSceneSystem::onExit(cgl::ECSCore* pECS) noexcept {
    cgl::InitSceneRenderDestroySystem destroySys;

    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &destroySys,
            .pEcs        = pECS,
            .pCheckState = pSceneRenderState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pSceneRenderState_,
                .to     = cgl::StateTypes::RELEASE
            },
            .pOnErrorState = pSceneState_})) {
        return;
    }
}

// -----------------------------------------------------------------------------
void InitSceneSystem::update(cgl::ECSCore* pECS) noexcept {
    // render
    renderDrawSys_.update(pECS);
    if (CGL_IS_STATE_ERROR(pSceneRenderState_)) {
        cgl::RaiseError(pSceneState_, pSceneRenderState_->lastError);
        return;
    }

    // check resource init finish
    assetsReaderInitSys_.update(pECS);
    if (CGL_IS_STATE_ERROR(pAssetsReaderState_)) {
        cgl::RaiseError(pSceneState_, pAssetsReaderState_->lastError);
        return;
    }
}

// -----------------------------------------------------------------------------
cgl::IScene::Ptr cgl::CreateInitScene() {
    return std::make_unique<InitSceneSystem>();
}
