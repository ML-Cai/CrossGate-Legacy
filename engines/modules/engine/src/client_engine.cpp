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
#include "ecs.h"
#include "engine_components.h"
#include "window/window_components.h"
#include "window/window_init_system.h"
#include "window/window_input_system.h"

// -----------------------------------------------------------------------------
namespace {

class ClientEngine : public cgl::IEngine {
 public:
    explicit ClientEngine(cgl::SettingsPtr setting);

    ~ClientEngine();

    void run() override;

 private:
    void init();

    cgl::WindowInitSystem winInitSys_;
    cgl::WindowInputSystem winInputSys_;


    cgl::SettingsPtr setting_;
    cgl::ECSCore ecs_;
    cgl::ECSCore& ecs() noexcept { return ecs_; }

};

}   // namespace


ClientEngine::ClientEngine(cgl::SettingsPtr setting)
    : setting_(std::move(setting))  {
}

ClientEngine::~ClientEngine() {
}

void ClientEngine::init() {
    LOGD("Init client engine");

    // append all essential singleton components
    ecs().addSingleton<cgl::component::EngineState>(
        cgl::EngineStateTypes::UNKNOWN, "");

    ecs().addSingleton<cgl::component::WindowCreateInfo>(
        800, 600, "CGL");
}

void ClientEngine::run() {
    LOGD("Run client engine");
    init();

    // init window
    winInitSys_.update(&ecs_);

    // main loop
    auto pWinHandle = ecs().getSingleton<cgl::component::WindowHandle>();
    assert(pWinHandle != nullptr);

    while (pWinHandle->windowShouldClose == false) {
        winInputSys_.update(&ecs_);
    }
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