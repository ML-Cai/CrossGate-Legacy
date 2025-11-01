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
#include "scene/scene_components.h"
#include "scene/scene_systems.h"

void cgl::SceneManagerDestroySystem::update(cgl::ECSCore* pECS) {
    auto pSceneState = pECS->getSingleton<cgl::component::SceneState>();
    if (pSceneState == nullptr) {
        return;
    }

    // release scene
    if (pSceneState->scene != nullptr) {
        pSceneState->scene->onExit(pECS);
        pSceneState->scene = nullptr;
    }

    pSceneState->nextScene = cgl::SceneTypes::NullScene;
}
