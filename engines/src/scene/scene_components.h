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
#include <string>
#include "cgl/common/viewport.h"

namespace cgl {

class ECSCore;
namespace graphics { class ICommandBuffer; }

// -----------------------------------------------------------------------------
class IScene {
 public:
    using Ptr = std::unique_ptr<cgl::IScene>;

    explicit IScene(cgl::SceneTypes sceneType) : sceneType_(sceneType) {}

    virtual ~IScene() = default;

    virtual void onEnter(cgl::ECSCore* pECS) noexcept = 0;

    virtual void onExit(cgl::ECSCore* pECS) noexcept = 0;

    virtual void update(cgl::ECSCore* pECS) noexcept = 0;

    // Record scene content inside the engine-owned render pass.
    virtual void draw(ECSCore*, graphics::ICommandBuffer*, const Viewport&) noexcept {}

    cgl::SceneTypes sceneType() const noexcept { return sceneType_; }

 private:
    const cgl::SceneTypes sceneType_;
};

cgl::IScene::Ptr CreateInitScene();
class AnimationLibrary;
cgl::IScene::Ptr CreateAnimationScene(AnimationLibrary& library);
class MapLibrary;
cgl::IScene::Ptr CreateMapScene(MapLibrary& maps, AnimationLibrary& animations);


namespace component {

// -----------------------------------------------------------------------------
struct SceneLoadProgress {
    uint32_t completed=0,total=0;
    uint64_t requiredBytes=0;
    bool ready=false;
};
struct SceneState {
    cgl::StateTypes state;
    cgl::SceneTypes nextScene;
    cgl::IScene::Ptr scene;
    std::string lastError;
};

}   // namespace component
}   // namespace cgl
