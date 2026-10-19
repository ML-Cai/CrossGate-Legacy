// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "assert.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <chrono>
#include "cgl/settings/settings.h"
#include "cgl/engine/engine.h"
#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/state_system.h"
#include "engine/engine_components.h"
#include "assets/assets_components.h"
#include "assets/animation_library.h"
#include "assets/map_library.h"
#include "window/window_components.h"
#include "window/window_systems.h"
#include "render/render_components.h"
#include "render/render_systems.h"
#include "render/frame_render_systems.h"
#include "scene/scene_systems.h"
#include "scene/scene_components.h"

// -----------------------------------------------------------------------------
namespace {

class ClientEngine : public cgl::IEngine {
 public:
    explicit ClientEngine(cgl::SettingsPtr setting);

    ~ClientEngine();

    bool run(uint32_t frameLimit) override;

 private:
    void initSingletonStage(cgl::ECSCore*);
    void initModulesStage(cgl::ECSCore*);
    void runStage(cgl::ECSCore*, uint32_t frameLimit);
    void closeStage(cgl::ECSCore*);
    bool anyStateError() const noexcept;

    cgl::SettingsPtr setting_;
    cgl::ECSCore ecs_;
    cgl::ECSCore& ecs() noexcept { return ecs_; }

    cgl::WindowInitSystem winInitSys_;
    cgl::WindowInputSystem winInputSys_;
    cgl::RenderDeviceInitSystem renderDeviceInitSys_;
    cgl::FrameRenderDrawSystem renderDrawSys_;
    cgl::AnimationLibrary animationLibrary_;
    cgl::MapLibrary mapLibrary_;
    cgl::SceneManagerSystem sceneMgrSys_{[this](cgl::SceneTypes type) {
        return cgl::CreateScene(type, animationLibrary_, &mapLibrary_);
    }};

    // state objects
    cgl::component::EngineState* pEngineState_;
    cgl::component::WindowState* pWindowState_;
    cgl::component::SceneState* pSceneState_;
    cgl::component::RenderDeviceState* pRenderDevState_;
};

}   // namespace


// -----------------------------------------------------------------------------
ClientEngine::ClientEngine(cgl::SettingsPtr setting)
    : setting_(std::move(setting))  {
}

// -----------------------------------------------------------------------------
ClientEngine::~ClientEngine() {
}

// -----------------------------------------------------------------------------
void ClientEngine::initSingletonStage(cgl::ECSCore* pEcs) {
    // add engine state
    ecs().addSingleton<cgl::component::EngineState>(
        cgl::StateTypes::UNKNOWN, "", std::move(setting_));
    pEngineState_ = ecs().getSingleton<cgl::component::EngineState>();

    // add window state
    ecs().addSingleton<cgl::component::WindowState>(
        cgl::StateTypes::UNKNOWN, "");
    pWindowState_ = ecs().getSingleton<cgl::component::WindowState>();

    // add scene state
    ecs().addSingleton<cgl::component::SceneState>(
        cgl::StateTypes::UNKNOWN, cgl::SceneTypes::Unknown, nullptr, "");
    pSceneState_ = ecs().getSingleton<cgl::component::SceneState>();

    // add assets reader state
    ecs().addSingleton<cgl::component::AssetsReaderState>(
        cgl::StateTypes::UNKNOWN, "");

    // add render state
    ecs().addSingleton<cgl::component::RenderDeviceState>(
        cgl::StateTypes::UNKNOWN, "");
    pRenderDevState_ = ecs().getSingleton<cgl::component::RenderDeviceState>();
    ecs().addSingleton<cgl::component::FrameRenderState>(cgl::StateTypes::INITIALIZING, "");
    ecs().addSingleton<cgl::component::KeyboardInputState>();
    ecs().addSingleton<cgl::component::CursorPositionState>();
    ecs().addSingleton<cgl::component::FrameTime>();

    // add window create info
    ecs().addSingleton<cgl::component::WindowCreateInfo>(
        800, 600, "CGL");
}

// -----------------------------------------------------------------------------
void ClientEngine::initModulesStage(cgl::ECSCore* pEcs) {
    // init window
    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &winInitSys_,
            .pEcs        = pEcs,
            .pCheckState = pWindowState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pWindowState_,
                .to     = cgl::StateTypes::RUNNING
            },
            .pOnErrorState = pEngineState_})) {
        return;
    }

    // init render device
    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &renderDeviceInitSys_,
            .pEcs        = pEcs,
            .pCheckState = pRenderDevState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pRenderDevState_,
                .to     = cgl::StateTypes::RUNNING
            },
            .pOnErrorState = pEngineState_})) {
        return;
    }

    cgl::FrameRenderInitSystem frameInit;
    frameInit.update(pEcs);
    auto* frame = pEcs->getSingleton<cgl::component::FrameRenderState>();
    if (frame->state == cgl::StateTypes::ERROR) {
        cgl::RaiseError(pEngineState_, frame->lastError);
        return;
    }
    frame->state = cgl::StateTypes::RUNNING;

    // init scene manager
    if (!CGL_UPDATE_SYS_AND_TRANSIT_STATE({
            .pSystem     = &sceneMgrSys_,
            .pEcs        = pEcs,
            .pCheckState = pSceneState_,
            .onSuccess   = cgl::StateTransitionHint{
                .pState = pSceneState_,
                .to     = cgl::StateTypes::RUNNING
            },
            .pOnErrorState = pEngineState_})) {
        return;
    }
}

// -----------------------------------------------------------------------------
void ClientEngine::runStage(cgl::ECSCore* pEcs, uint32_t frameLimit) {
    // main loop
    auto pWinHandle = ecs().getSingleton<cgl::component::WindowHandle>();
    assert(pWinHandle != nullptr);

    uint32_t frames = 0;
    auto previous = std::chrono::steady_clock::now();
    while (!pWinHandle->windowShouldClose && (!frameLimit || frames < frameLimit)) {
        const auto now = std::chrono::steady_clock::now();
        pEcs->getSingleton<cgl::component::FrameTime>()->deltaSeconds =
            std::chrono::duration<double>(now - previous).count();
        previous = now;
        if (anyStateError()) break;
        winInputSys_.update(pEcs);
        if (pWinHandle->windowShouldClose) break;

        if (anyStateError()) break;
        sceneMgrSys_.update(pEcs);
        if (anyStateError()) break;
        renderDrawSys_.update(pEcs);
        auto* frame = pEcs->getSingleton<cgl::component::FrameRenderState>();
        if (frame->state == cgl::StateTypes::ERROR) {
            cgl::RaiseError(pEngineState_, frame->lastError);
            break;
        }
        ++frames;
    }
}

// -----------------------------------------------------------------------------
void ClientEngine::closeStage(cgl::ECSCore* pEcs) {
    if (pRenderDevState_->pDevice) pRenderDevState_->pDevice->waitIdle();
    // destroy scene
    cgl::SceneManagerDestroySystem sceneMgrDestroySys;
    sceneMgrDestroySys.update(pEcs);
    animationLibrary_.clear();
    mapLibrary_.clear();

    cgl::FrameRenderDestroySystem frameDestroy;
    frameDestroy.update(pEcs);

    // destory render device
    cgl::RenderDeviceDestroySystem renderDeviceDestroySys;
    renderDeviceDestroySys.update(pEcs);

    if (auto window = pEcs->getSingleton<cgl::component::WindowHandle>()) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window->nativeHandle));
        pEcs->removeSingleton<cgl::component::WindowHandle>();
    }
    glfwTerminate();
}

// -----------------------------------------------------------------------------
bool ClientEngine::anyStateError() const noexcept {
    return (CGL_IS_STATE_ERROR(pEngineState_) ||
            CGL_IS_STATE_ERROR(pWindowState_) ||
            CGL_IS_STATE_ERROR(pRenderDevState_) ||
            CGL_IS_STATE_ERROR(pSceneState_));
}

// -----------------------------------------------------------------------------
bool ClientEngine::run(uint32_t frameLimit) {
    LOGI("Run client engine");

    initSingletonStage(&ecs_);

    if (anyStateError() == false) {
        initModulesStage(&ecs_);
    }

    if (anyStateError() == false) {
        runStage(&ecs_, frameLimit);
    }

    const bool success = !anyStateError();
    closeStage(&ecs_);

    LOGI("Engine close");
    return success;
}

// -----------------------------------------------------------------------------
// cgl::IEngine
cgl::IEngine::Ptr cgl::IEngine::create(const std::string_view setting_path) {
    auto pSetting = cgl::LoadSettings(setting_path);
    if (pSetting == nullptr) {
        LOGE("Failed to load settings from : " << setting_path);
        return nullptr;
    }
    const std::filesystem::path root(pSetting->general.EngineRootPath);
    if (root.is_relative() && !setting_path.empty()) {
        pSetting->general.EngineRootPath =
            (std::filesystem::absolute(std::filesystem::path(setting_path)).parent_path() / root).string();
    }
    cgl::SetLogLevel(cgl::LogLevel::Trace);

    return std::make_unique<ClientEngine>(std::move(pSetting));
}
