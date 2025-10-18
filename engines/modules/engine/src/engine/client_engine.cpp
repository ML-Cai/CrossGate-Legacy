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
    void init();
    cgl::SettingsPtr setting_;
    cgl::ECSCore ecs_;
    cgl::ECSCore& ecs() noexcept { return ecs_; }

    cgl::WindowInitSystem winInitSys_;
    cgl::WindowInputSystem winInputSys_;
    cgl::RenderDeviceInitSystem renderDeviceInitSys_;
    cgl::SceneManagerSystem sceneMgrSys_;

    // state objects
    cgl::component::WindowState* pWindowState_;
    cgl::component::SceneState* pSceneState_;
    cgl::component::RenderState* pRenderState_;
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
void ClientEngine::init() {
    LOGI("Init client engine");

    // add engine state
    auto pSettings = cgl::LoadSettings("settings.ini");
    if (pSettings == nullptr) {
        LOGW("Failed to to the setting file, use default setting for the engine");
        pSettings = cgl::LoadSettings();
    }
    ecs().addSingleton<cgl::component::EngineState>(
        cgl::StateTypes::UNKNOWN, "", std::move(pSettings));

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
    ecs().addSingleton<cgl::component::RenderState>(
        cgl::StateTypes::UNKNOWN, "");
    pRenderState_ = ecs().getSingleton<cgl::component::RenderState>();

    // add window create info
    ecs().addSingleton<cgl::component::WindowCreateInfo>(
        800, 600, "CGL");
}

// -----------------------------------------------------------------------------
void ClientEngine::run() {
    LOGI("Run client engine");
    init();

    // init window
    winInitSys_.update(&ecs_);
    renderDeviceInitSys_.update(&ecs_);

    // setup init scene
    pSceneState_->nextScene = cgl::SceneTypes::InitScene;


    // main loop
    auto pWinHandle = ecs().getSingleton<cgl::component::WindowHandle>();
    assert(pWinHandle != nullptr);

    while (pWinHandle->windowShouldClose == false) {
        winInputSys_.update(&ecs_);

        sceneMgrSys_.update(&ecs_);

        // check error
        if ((pWindowState_->state == cgl::StateTypes::ERROR) ||
            (pRenderState_->state == cgl::StateTypes::ERROR) ||
            (pSceneState_->state == cgl::StateTypes::ERROR)) {
            break;
        }
    }

    // destroy scene
    cgl::SceneManagerDestroySystem sceneMgrDestroySys;
    sceneMgrDestroySys.update(&ecs_);

    // destory window
    cgl::RenderDeviceDestroySystem renderDeviceDestroySys;
    renderDeviceDestroySys.update(&ecs_);
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
