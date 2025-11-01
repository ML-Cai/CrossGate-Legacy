// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

namespace cgl {

class ECSCore;

namespace component {

struct SceneState;

}   // namespace component

// -----------------------------------------------------------------------------
class SceneManagerSystem {
 public:
    SceneManagerSystem();

    ~SceneManagerSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void initStage(cgl::ECSCore* pECS);
    void sceneUpdateStage(cgl::ECSCore* pECS);

    void switchScene(cgl::ECSCore* pECS);

    using UpdateFunc = void (SceneManagerSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    cgl::component::SceneState* pSceneState_;

};

// -----------------------------------------------------------------------------
class SceneManagerDestroySystem {
 public:
    SceneManagerDestroySystem() = default;

    ~SceneManagerDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};


}   // namespace
