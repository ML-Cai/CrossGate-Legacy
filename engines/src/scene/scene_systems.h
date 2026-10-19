// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once
#include <functional>
#include "scene/scene_components.h"

namespace cgl {

class ECSCore;
IScene::Ptr CreateScene(SceneTypes type);

namespace component {

struct SceneState;

}   // namespace component

// -----------------------------------------------------------------------------
class SceneManagerSystem {
 public:
    using SceneFactory = std::function<IScene::Ptr(SceneTypes)>;
    explicit SceneManagerSystem(SceneFactory factory = CreateScene);

    ~SceneManagerSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void initStage(cgl::ECSCore* pECS);
    void sceneUpdateStage(cgl::ECSCore* pECS);

    void switchScene(cgl::ECSCore* pECS);

    using UpdateFunc = void (SceneManagerSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    cgl::component::SceneState* pSceneState_;
    SceneFactory factory_;

};

// -----------------------------------------------------------------------------
class SceneManagerDestroySystem {
 public:
    SceneManagerDestroySystem() = default;

    ~SceneManagerDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};


}   // namespace
