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
cgl::SceneManagerSystem::SceneManagerSystem() {
    updater_ = &cgl::SceneManagerSystem::init;
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::init(cgl::ECSCore* pECS) {
    pSceneState_ = pECS->getSingleton<cgl::component::SceneState>();
    assert(pSceneState_ != nullptr);
    updater_ = &cgl::SceneManagerSystem::_update;
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::switchScene(cgl::ECSCore* pECS) {
    if ((pSceneState_->scene == nullptr) ||
        (pSceneState_->scene->sceneType() != pSceneState_->nextScene)) {

        // handle exit event of previous scene.
        if (pSceneState_->scene != nullptr) {
            pSceneState_->scene->onExit(pECS);
        }

        // create new scene
        cgl::IScene::Ptr scene;
        switch (pSceneState_->nextScene) {
        case cgl::SceneTypes::InitScene:
            scene = cgl::CreateInitScene();
            break;
        default:
            cgl::RaiseError(pSceneState_,
                "Failed to create new scene: ", pSceneState_->nextScene);
            return;
        }

        scene->onEnter(pECS);
        pSceneState_->scene = std::move(scene);
    }

    pSceneState_->nextScene = cgl::SceneTypes::Unknown;
}

// -----------------------------------------------------------------------------
void cgl::SceneManagerSystem::_update(cgl::ECSCore* pECS) {
    // switch scene?
    if (pSceneState_->nextScene != cgl::SceneTypes::Unknown) {
        switchScene(pECS);
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
