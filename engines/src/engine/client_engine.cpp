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
#include <fstream>
#include "telemetry/frame_telemetry.h"
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
    },[this] {
        const auto& policy=pEngineState_->settings->cachemaintenance;
        if(!policy.Enabled) return;
        auto* sync=ecs_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
        const auto submitted=sync?sync->progress.submitted:0;
        const auto animations=animationLibrary_.retireUnused(uint64_t(policy.AnimationTargetMiB)*1024*1024,submitted);
        const auto maps=mapLibrary_.retireUnused(uint64_t(policy.MapTargetMiB)*1024*1024,submitted);
        if(animations || maps) LOGI("Scene cache retirement: animation bytes=" << animations << ", map/outline bytes=" << maps);
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

    if(pEngineState_->settings->telemetry.Enabled)
        ecs().addSingleton<cgl::component::RuntimeTelemetry>();
    else if(!pEngineState_->settings->telemetry.ReportPath.empty())
        cgl::RaiseError(pEngineState_, "Telemetry ReportPath requires Enabled=true");
    const auto& policy=pEngineState_->settings->cachemaintenance;
    if(policy.AnimationTargetMiB>1024 || policy.MapTargetMiB>1024)
        cgl::RaiseError(pEngineState_, "Cache maintenance targets must be at most 1024 MiB");
    const auto& bench = pEngineState_->settings->benchmark;
    const bool pressure = bench.ActorCount || bench.ActorsPerCell;
    const bool serverDemo=pEngineState_->settings->serverdemo.Enabled;
    if(serverDemo) {
        if(pressure || pEngineState_->settings->map.FilePath.empty())
            cgl::RaiseError(pEngineState_, "ServerDemo requires a map and cannot run with Benchmark");
        animationLibrary_.setTextureBudget(512ull*1024*1024);
    }
    if (pressure) {
        if ((bench.BackgroundDecode && (!bench.LoadDuringPlayback || !bench.ProgressiveUploads)) || bench.AnimationBudgetMiB<16 || bench.AnimationBudgetMiB>1024 || pEngineState_->settings->map.FilePath.empty() || !bench.SampleFrames || bench.SampleFrames > 100000 || bench.WarmupFrames > 100000 ||
            bench.ReportPath.empty() || bench.Width < 320 || bench.Width > 3840 || bench.Height < 240 || bench.Height > 2160)
            cgl::RaiseError(pEngineState_, "Invalid benchmark limits/report/viewport");
        else {
            animationLibrary_.setTextureBudget(uint64_t(bench.AnimationBudgetMiB)*1024*1024);
            ecs().addSingleton<cgl::component::CrowdBenchmark>();
        }
    }
    ecs().addSingleton<cgl::component::WindowCreateInfo>(
        pressure ? bench.Width : (serverDemo ? 1280 : 800), pressure ? bench.Height : (serverDemo ? 720 : 600), "CGL");
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
    auto* telemetry=pEcs->getSingleton<cgl::component::RuntimeTelemetry>();
    auto previous = std::chrono::steady_clock::now();
    auto lastReport=previous;
    auto emitTelemetry=[&](std::ostream& out) {
        const auto summary=telemetry->frames.summary();
        const auto animations=animationLibrary_.usage();const auto maps=mapLibrary_.usage();
        const auto* swap=pRenderDevState_->pSwapchain.get();
        out << "{\n\"status\":\"" << (anyStateError()?"scene_error":"complete") << "\","
            << "\"present_mode\":\"" << (swap?swap->presentModeName():"unavailable") << "\","
            << "\"window_capacity\":" << cgl::FrameTelemetry::Capacity << ",\"window_samples\":" << summary.count
            << ",\"total_samples\":" << summary.total << ",\"fps\":" << summary.fps
            << ",\"frame_mean_ms\":" << summary.meanMs << ",\"frame_p50_ms\":" << summary.p50Ms
            << ",\"frame_p95_ms\":" << summary.p95Ms << ",\"frame_p99_ms\":" << summary.p99Ms
            << ",\"frame_max_ms\":" << summary.maxMs << ",\"frames_over_33ms\":" << summary.over33ms
            << ",\"update_mean_ms\":" << summary.updateMs << ",\"render_loop_mean_ms\":" << summary.renderMs
            << ",\"sort_mean_ms\":" << summary.sortMs << ",\"drawn_actors_last_frame\":" << telemetry->rendering.actors
            << ",\"draw_calls_last_frame\":" << telemetry->rendering.drawCalls
            << ",\"animation_clips\":" << animations.clips << ",\"animation_rgba_bytes\":" << animations.rgbaBytes
            << ",\"animation_retired_bytes\":" << animations.retiredBytes
            << ",\"animation_retired_clips\":" << animations.retiredClips
            << ",\"animation_reused_clips\":" << animations.reusedClips
            << ",\"map_reuses\":" << maps.reusedMaps << ",\"outline_reuses\":" << maps.reusedOutlines
            << ",\"animation_budget_deferrals\":" << animations.budgetDeferrals
            << ",\"animation_waiting_for_budget\":" << (animations.waitingForBudget?"true":"false")
            << ",\"animation_evictions\":" << animations.evictions << ",\"animation_evicted_bytes\":" << animations.evictedBytes
            << ",\"map_retired_bytes\":" << maps.retiredMapBytes
            << ",\"outline_retired_bytes\":" << maps.retiredOutlineBytes
            << ",\"retired_maps\":" << maps.retiredMaps << ",\"retired_outlines\":" << maps.retiredOutlines
            << ",\"map_outline_evictions\":" << maps.evictions
            << ",\"animation_pending_reserved_bytes\":" << animations.pendingBytes << ",\"animation_budget_bytes\":" << animations.budgetBytes
            << ",\"cached_maps\":" << maps.maps << ",\"map_pages\":" << maps.pages
            << ",\"map_rgba_bytes\":" << maps.rgbaBytes << ",\"outline_rgba_bytes\":" << maps.outlineBytes << "\n}\n";
    };
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
        const bool wasCold = wasReady && settings.benchmark.LoadDuringPlayback &&
            benchmark->unique < settings.benchmark.UniqueCharacters;
        const bool wasRunning=pSceneState_->scene && pSceneState_->scene->sceneType()!=cgl::SceneTypes::InitScene;
        if(auto* sync=pEcs->getSingleton<cgl::component::PrimaryRenderSyncObjects>()) {
            animationLibrary_.maintainStreaming(sync->progress.submitted,sync->progress.completed,settings.cachemaintenance.Enabled);
            mapLibrary_.collectRetired(sync->progress.completed,1,2);
        }
        sceneMgrSys_.update(pEcs);
        const auto updateEnd = std::chrono::steady_clock::now();
        if (anyStateError()) break;
        if(telemetry) telemetry->rendering={};
        renderDrawSys_.update(pEcs);
        auto* frame = pEcs->getSingleton<cgl::component::FrameRenderState>();
        if (frame->state == cgl::StateTypes::ERROR) {
            cgl::RaiseError(pEngineState_, frame->lastError);
            break;
        }
        const auto end = std::chrono::steady_clock::now();
        if(telemetry && wasRunning) {
            auto ms=[](auto duration){return std::chrono::duration<double,std::milli>(duration).count();};
            if(end-lastReport>=std::chrono::seconds(2)) {
                const auto summary=telemetry->frames.summary();
                const auto usage=animationLibrary_.usage();const auto maps=mapLibrary_.usage();
                LOGI("Telemetry: FPS=" << summary.fps << ", p99=" << summary.p99Ms << " ms, clips=" << usage.clips
                    << ", animation RGBA=" << usage.rgbaBytes << ", pending reserved=" << usage.pendingBytes
                    << ", map RGBA=" << maps.rgbaBytes);
                lastReport=end;
            }
            auto sample=telemetry->rendering;
            sample.frameMs=ms(std::chrono::steady_clock::now()-now);sample.updateMs=ms(updateEnd-updateStart);sample.renderMs=ms(end-updateEnd);
            telemetry->frames.add(sample);
        }
        if (benchmark && wasReady) {
            auto ms=[](auto span){ return std::chrono::duration<double,std::milli>(span).count(); };
            samples.add(ms((telemetry?std::chrono::steady_clock::now():end)-now),ms(updateEnd-updateStart),ms(end-updateEnd),benchmark->sortMs,
                        settings.benchmark.WarmupFrames,wasCold);
            if (samples.frame.size() >= settings.benchmark.SampleFrames) break;
        }
        ++frames;
    }
    if(telemetry && !settings.telemetry.ReportPath.empty()) {
        std::ofstream report(settings.telemetry.ReportPath);
        if(!report) throw std::runtime_error("Cannot open telemetry report");
        emitTelemetry(report);report.flush();
        if(!report) throw std::runtime_error("Cannot write telemetry report");
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
