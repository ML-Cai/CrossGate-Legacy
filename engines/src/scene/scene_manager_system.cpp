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
#include "cgl/common/formatters.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "scene/scene_components.h"
#include "scene/scene_systems.h"

// -----------------------------------------------------------------------------
cgl::IScene::Ptr cgl::CreateScene(SceneTypes type, AnimationLibrary& library, MapLibrary* maps) {
    switch (type) {
    case SceneTypes::InitScene: return CreateInitScene();
    case SceneTypes::AnimationScene: return CreateAnimationScene(library);
    case SceneTypes::MapScene: return maps ? CreateMapScene(*maps, library) : nullptr;
    default: return nullptr;
    }
}

cgl::SceneManagerSystem::SceneManagerSystem(AnimationLibrary& library)
    : SceneManagerSystem([&library](SceneTypes type) { return CreateScene(type, library); }) {}

cgl::SceneManagerSystem::SceneManagerSystem(SceneFactory factory)
    : factory_(std::move(factory)) {
    updater_ = &cgl::SceneManagerSystem::initStage;
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::initStage(cgl::ECSCore* pECS) {
    pSceneState_ = pECS->getSingleton<cgl::component::SceneState>();
    assert(pSceneState_ != nullptr);

    // setup init scene
    pSceneState_->nextScene = cgl::SceneTypes::InitScene;

    // swtich to next stage
    updater_ = &cgl::SceneManagerSystem::sceneUpdateStage;
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::switchScene(cgl::ECSCore* pECS) {
    // Consume first so a request made by onEnter survives until the next update.
    const auto requested = pSceneState_->nextScene;
    pSceneState_->nextScene = cgl::SceneTypes::Unknown;
    cgl::IScene::Ptr next;
    try {
        next = factory_(requested);
    } catch (const std::exception& error) {
        cgl::RaiseError(pSceneState_, "Scene creation failed: ", error.what());
        return;
    }
    if (!next || next->sceneType() != requested) {
        cgl::RaiseError(pSceneState_, "Unsupported scene: ", requested);
        return;
    }
    // Validate the destination before exiting the current scene. Requesting the
    // current type deliberately restarts it through the same exit/entry path.
    if (pSceneState_->scene) {
        pSceneState_->scene->onExit(pECS);
        pSceneState_->scene.reset();
        if (pSceneState_->state == cgl::StateTypes::ERROR) return;
    }
    pSceneState_->scene = std::move(next);
    pSceneState_->scene->onEnter(pECS);

}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::sceneUpdateStage(cgl::ECSCore* pECS) {
    // switch scene?
    if (pSceneState_->nextScene != cgl::SceneTypes::Unknown) {
        switchScene(pECS);

        if (pSceneState_->state == cgl::StateTypes::ERROR) {
            return;
        }
    }

    // update scene
    if (pSceneState_->scene != nullptr) {
        pSceneState_->scene->update(pECS);
    }
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}
