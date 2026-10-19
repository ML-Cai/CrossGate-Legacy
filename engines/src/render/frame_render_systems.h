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

constexpr char PRIMARY_RENDER_PASS[] = "PRIMARY_RENDER_PASS";

// -----------------------------------------------------------------------------
class FrameRenderInitSystem {
 public:
    FrameRenderInitSystem() = default;

    ~FrameRenderInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool initEssentialRenderObjects(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class FrameRenderDestroySystem {
 public:
    FrameRenderDestroySystem() = default;

    ~FrameRenderDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class FrameRenderDrawSystem {
 public:
    FrameRenderDrawSystem();

    ~FrameRenderDrawSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    void init(cgl::ECSCore* pECS);
    void render(cgl::ECSCore* pECS);

    using UpdateFunc = void (FrameRenderDrawSystem::*)(cgl::ECSCore*);
    UpdateFunc updater_;

    cgl::component::FrameRenderState* pFrameRenderState_;
    cgl::component::RenderDeviceState* pRenderDeviceState_;
    cgl::component::WindowHandle* pWindowHandle_;
    cgl::component::PrimarySceneRenderFrame* pFrame_;
    cgl::component::PrimaryRenderSyncObjects* pSyncObjs_;

    cgl::component::RenderSyncObjects* acquireNextSyncObj() noexcept;
    uint32_t frameIdx_;
};

}   // namespace cgl
