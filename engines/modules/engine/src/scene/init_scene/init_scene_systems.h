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
class InitSceneInitRenderSystem {
 public:
    InitSceneInitRenderSystem() = default;

    ~InitSceneInitRenderSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool initEssentialRenderObjects(cgl::ECSCore* pECS);

    cgl::component::SceneState* pState_;
};

// -----------------------------------------------------------------------------
class InitSceneDestroySystem {
 public:
    InitSceneDestroySystem() = default;

    ~InitSceneDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class InitSceneRenderSystem {
 public:
    InitSceneRenderSystem();

    ~InitSceneRenderSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void init(cgl::ECSCore* pECS);
    void render(cgl::ECSCore* pECS);

    using UpdateFunc = void (InitSceneRenderSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    cgl::component::SceneState* pState_;
    cgl::component::WindowHandle* pWindowHandle_;
    cgl::component::PrimaryDeviceContext* pDevCtx_;
    cgl::component::PrimarySwapchain* pSwapchain_;
    cgl::component::PrimarySceneRenderFrame* pFrame_;
    cgl::component::PrimaryRenderSyncObjects* pSyncObjs_;

    cgl::component::RenderSyncObjects* acquireNextSyncObj() noexcept;
    uint32_t frameIdx_;
};

}   // namespace cgl
