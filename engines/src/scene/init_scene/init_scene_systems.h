// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "scene/scene_components.h"
#include "window/window_components.h"
#include "render/render_components.h"

// -----------------------------------------------------------------------------
namespace cgl {

class ECSCore;

constexpr char INIT_SCENE_BASIC_RENDER_PASS[] = "INIT_SCENE_BASIC_RENDER_PASS";

// -----------------------------------------------------------------------------
class InitSceneRenderInitSystem {
 public:
    InitSceneRenderInitSystem() = default;

    ~InitSceneRenderInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool initEssentialRenderObjects(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class InitSceneRenderDestroySystem {
 public:
    InitSceneRenderDestroySystem() = default;

    ~InitSceneRenderDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class InitSceneRenderDrawSystem {
 public:
    InitSceneRenderDrawSystem();

    ~InitSceneRenderDrawSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void init(cgl::ECSCore* pECS);
    void render(cgl::ECSCore* pECS);

    using UpdateFunc = void (InitSceneRenderDrawSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    cgl::component::SceneRenderState* pSceneRenderState_;
    cgl::component::RenderDeviceState* pRenderDeviceState_;
    cgl::component::WindowHandle* pWindowHandle_;
    cgl::component::PrimarySceneRenderFrame* pFrame_;
    cgl::component::PrimaryRenderSyncObjects* pSyncObjs_;

    cgl::component::RenderSyncObjects* acquireNextSyncObj() noexcept;
    uint32_t frameIdx_;
};

}   // namespace cgl
