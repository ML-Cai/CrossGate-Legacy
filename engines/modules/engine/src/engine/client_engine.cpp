// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "assert.h"
#include "cgl/settings/settings.h"
#include "cgl/engine/engine.h"
#include "cgl/trace/logger.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/state_system.h"
#include "engine/engine_components.h"
#include "assets/assets_components.h"
#include "window/window_components.h"
#include "window/window_systems.h"
#include "render/render_components.h"
#include "render/render_systems.h"
#include "scene/scene_systems.h"
#include "scene/scene_components.h"

// -----------------------------------------------------------------------------
namespace {

class ClientEngine : public cgl::IEngine {
 public:
    explicit ClientEngine(cgl::SettingsPtr setting);

    ~ClientEngine();

    void run() override;

 private:
    void initSingletonStage(cgl::ECSCore*);
    void initModulesStage(cgl::ECSCore*);
    void runStage(cgl::ECSCore*);
    void closeStage(cgl::ECSCore*);
    bool anyStateError() const noexcept;

    cgl::SettingsPtr setting_;
    cgl::ECSCore ecs_;
    cgl::ECSCore& ecs() noexcept { return ecs_; }

    cgl::WindowInitSystem winInitSys_;
    cgl::WindowInputSystem winInputSys_;
    cgl::RenderDeviceInitSystem renderDeviceInitSys_;
    cgl::SceneManagerSystem sceneMgrSys_;

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
    auto pSettings = cgl::LoadSettings("settings.ini");
    if (pSettings == nullptr) {
        LOGW("Failed to to the setting file, use default setting for the engine");
        pSettings = cgl::LoadSettings();
    }
    ecs().addSingleton<cgl::component::EngineState>(
        cgl::StateTypes::UNKNOWN, "", std::move(pSettings));
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
void ClientEngine::runStage(cgl::ECSCore* pEcs) {
    // main loop
    auto pWinHandle = ecs().getSingleton<cgl::component::WindowHandle>();
    assert(pWinHandle != nullptr);

    while (pWinHandle->windowShouldClose == false) {
        if (anyStateError()) break;
        winInputSys_.update(pEcs);

        if (anyStateError()) break;
        sceneMgrSys_.update(pEcs);
    }
}

// -----------------------------------------------------------------------------
void ClientEngine::closeStage(cgl::ECSCore* pEcs) {
    // destroy scene
    cgl::SceneManagerDestroySystem sceneMgrDestroySys;
    sceneMgrDestroySys.update(pEcs);

    // destory render device
    cgl::RenderDeviceDestroySystem renderDeviceDestroySys;
    renderDeviceDestroySys.update(pEcs);
}

// -----------------------------------------------------------------------------
bool ClientEngine::anyStateError() const noexcept {
    return (CGL_IS_STATE_ERROR(pWindowState_) ||
            CGL_IS_STATE_ERROR(pRenderDevState_) ||
            CGL_IS_STATE_ERROR(pSceneState_));
}

// -----------------------------------------------------------------------------
void ClientEngine::run() {
    LOGI("Run client engine");

    initSingletonStage(&ecs_);

    if (anyStateError() == false) {
        initModulesStage(&ecs_);
    }

    if (anyStateError() == false) {
        runStage(&ecs_);
    }

    closeStage(&ecs_);

    LOGI("Engine close");
}

// -----------------------------------------------------------------------------
// cgl::IEngine
cgl::IEngine::Ptr cgl::IEngine::create(const std::string_view setting_path) {
    auto pSetting = cgl::LoadSettings(setting_path);
    if (pSetting == nullptr) {
        LOGE("Failed to load settings from : " << setting_path);
        return nullptr;
    }
    cgl::SetLogLevel(cgl::LogLevel::Trace);

    return std::make_unique<ClientEngine>(std::move(pSetting));
}
