// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/common/scene_types.h"
#include "cgl/common/states.h"

namespace cgl {

struct ECSCore;

class IScene {
 public:
    using Ptr = std::unique_ptr<cgl::IScene>;

    explicit IScene(cgl::SceneTypes sceneType) : sceneType_(sceneType) {}

    virtual ~IScene() = default;

    virtual void onEnter(cgl::ECSCore* pECS) noexcept = 0;

    virtual void onExit(cgl::ECSCore* pECS) noexcept = 0;

    virtual void update(cgl::ECSCore* pECS) noexcept = 0;

    cgl::SceneTypes sceneType() const noexcept { return sceneType_; }

 private:
    const cgl::SceneTypes sceneType_;
};

cgl::IScene::Ptr CreateInitScene();


namespace component {

struct SceneState {
    cgl::StateTypes state;
    cgl::SceneTypes nextScene;
    cgl::IScene::Ptr scene;
    std::string lastError;
};

}   // namespace component
}   // namespace cgl
