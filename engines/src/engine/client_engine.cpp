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
#include "benchmark/crowd_benchmark.h"
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

    const auto& bench = pEngineState_->settings->benchmark;
    const bool pressure = bench.ActorCount || bench.ActorsPerCell;
    if (pressure) {
        if (bench.AnimationBudgetMiB<16 || bench.AnimationBudgetMiB>1024 || pEngineState_->settings->map.FilePath.empty() || !bench.SampleFrames || bench.SampleFrames > 100000 || bench.WarmupFrames > 100000 ||
            bench.ReportPath.empty() || bench.Width < 320 || bench.Width > 3840 || bench.Height < 240 || bench.Height > 2160)
            cgl::RaiseError(pEngineState_, "Invalid benchmark limits/report/viewport");
        else {
            animationLibrary_.setTextureBudget(uint64_t(bench.AnimationBudgetMiB)*1024*1024);
            ecs().addSingleton<cgl::component::CrowdBenchmark>();
        }
    }
    ecs().addSingleton<cgl::component::WindowCreateInfo>(
        pressure ? bench.Width : 800, pressure ? bench.Height : 600, "CGL");
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
    cgl::CrowdSamples samples;
    const auto& settings = *pEngineState_->settings;
    auto* benchmark = pEcs->getSingleton<cgl::component::CrowdBenchmark>();
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
        const auto updateStart = std::chrono::steady_clock::now();
        const bool wasReady = benchmark && benchmark->ready;
        sceneMgrSys_.update(pEcs);
        const auto updateEnd = std::chrono::steady_clock::now();
        if (anyStateError()) break;
        renderDrawSys_.update(pEcs);
        auto* frame = pEcs->getSingleton<cgl::component::FrameRenderState>();
        if (frame->state == cgl::StateTypes::ERROR) {
            cgl::RaiseError(pEngineState_, frame->lastError);
            break;
        }
        const auto end = std::chrono::steady_clock::now();
        if (benchmark && wasReady) {
            auto ms=[](auto span){ return std::chrono::duration<double,std::milli>(span).count(); };
            samples.add(ms(end-now),ms(updateEnd-updateStart),ms(end-updateEnd),benchmark->sortMs,
                        settings.benchmark.WarmupFrames);
            if (samples.frame.size() >= settings.benchmark.SampleFrames) break;
        }
        ++frames;
    }
    if (benchmark) {
        const auto* swap = pRenderDevState_->pSwapchain.get();
        const char* status = anyStateError() ? "scene_error" :
            samples.frame.size() == settings.benchmark.SampleFrames && benchmark->unique == settings.benchmark.UniqueCharacters ? "complete" : "incomplete";
        samples.write(settings,*benchmark,swap ? swap->presentModeName() : "unavailable",status);
        LOGI("Benchmark " << status << ": FPS=" << (samples.frame.empty()?0:1000/cgl::CrowdSamples::mean(samples.frame)));
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
        try { runStage(&ecs_, frameLimit); }
        catch (const std::exception& error) { cgl::RaiseError(pEngineState_, "Frame loop failed: ",error.what()); }
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
