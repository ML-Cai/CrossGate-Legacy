#include "map/server_demo_scenario.h"
#include "map/server_demo_refresh.h"
#include "assets/character_catalog.h"
#include "engine/engine_components.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include "telemetry/frame_telemetry.h"

namespace cgl {
void ServerDemoScenario::enter(ECSCore* world,const MapLayout& layout,Entity character) {
    width_=layout.width;height_=layout.height;character_=character;
    auto& readers=*world->getSingleton<component::AssetsReaderState>();
    const auto& options=world->getSingleton<component::EngineState>()->settings->serverdemo;
    ValidateDemoLimits(options.AnimationBudgetMiB,options.VisibleActors,options.PrefetchActors,options.CatalogLimit);
    auto catalog=cgl::ReadCharacterCatalog(readers,options.CatalogLimit);
    if(catalog.size()<uint64_t(options.VisibleActors)+options.PrefetchActors)
        throw std::runtime_error("Installed character catalog cannot provide distinct ServerDemo targets");
    if(options.StressFrames) stress_.emplace(options.StressFrames,options.StressLegFrames,options.StressReplaceFrames,options.StressStepMs);
    world->addSingleton<component::StreamingDiagnostics>();
    auto* stats=world->getSingleton<component::StreamingDiagnostics>();stats->configuredFrames=options.StressFrames;
    previousPosition_=world->getComponent<component::MapPosition>(character_)->cell;
    feed_=std::make_unique<cgl::LocalCharacterFeed>(catalog.size(),options.Seed,options.VisibleActors,options.PrefetchActors);
    remote_=std::make_unique<cgl::RemoteCharacters>(std::move(catalog));
    batchClock_=std::chrono::steady_clock::now();
    const auto& window=*world->getSingleton<cgl::component::WindowHandle>();
    sendSnapshot(world,{0,0,window.width,window.height},true);
}
StressMovement ServerDemoScenario::movement(ECSCore* world,double x,double y,double seconds) {
    auto& stats=*world->getSingleton<component::StreamingDiagnostics>();
    if(stress_ && stats.frames>=stress_->frames()) return {};
    ++stats.frames;
    movement_=stress_?stress_->at(stats.frames):StressMovement{x,y,seconds,false};return movement_;
}
void ServerDemoScenario::update(ECSCore* world,const Viewport& viewport) {
    if(!remote_ || !viewport.width || !viewport.height) return;
    auto* input=world->getSingleton<component::KeyboardInputState>();
    const auto now=std::chrono::steady_clock::now();
    const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
    auto& stats=*world->getSingleton<component::StreamingDiagnostics>();
    if(stress_ && stats.frames>=stress_->frames() && world->getSingleton<component::WindowHandle>()->windowShouldClose) return;
    const bool replace=stress_?movement_.replace:input->newBatchRequested;
    const double elapsed=stress_?(stats.frames-lastBatchFrame_)*movement_.seconds:std::chrono::duration<double>(now-batchClock_).count();
    if(ServerDemoRefreshDue(camera,lastBatchCenter_,viewport,elapsed,replace)) {
        const auto* telemetry=world->getSingleton<component::RuntimeTelemetry>();
        const auto summary=telemetry ? telemetry->frames.summary() : FrameSummary{};
        world->getSingleton<component::WindowHandle>()->pendingTitle=
            "CGL | " + (telemetry ? "FPS " + std::to_string(int(summary.fps)) + " | p99 " +
            std::to_string(summary.p99Ms) + " ms" : std::string("telemetry off")) + " | visible " + std::to_string(remote_->readyVisible) + "/" + std::to_string(remote_->visible) +
            " | prefetch " + std::to_string(remote_->readyOuter) + "/" + std::to_string(remote_->outer) +
            (stress_ ? " | scripted stress " + std::to_string(stats.frames) + "/" + std::to_string(stress_->frames()) : std::string(" | WASD move, N randomize, R restart"));
        sendSnapshot(world,viewport,replace);
        batchClock_=now;
    }
    input->newBatchRequested=false;
    auto& readers=*world->getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world->getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    remote_->update(*world,animations_,readers,device,pass,camera,viewport);
    stats.visible=remote_->visible;stats.readyVisible=remote_->readyVisible;
    stats.prefetch=remote_->outer;stats.readyPrefetch=remote_->readyOuter;
    stats.visibleSamples+=stats.visible;stats.readyVisibleSamples+=stats.readyVisible;
    if(stats.readyVisible<stats.visible) ++stats.visibleMissingFrames;
    stats.deferred=remote_->budgetDeferred;stats.peakDeferred=std::max(stats.peakDeferred,stats.deferred);
    if(stats.deferred) ++stats.deferredFrames;
    const auto usage=animations_.usage();if(usage.waitingForBudget) ++stats.budgetWaitFrames;
    stats.peakAccountedBytes=std::max(stats.peakAccountedBytes,usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes);
    stats.peakRetiredBytes=std::max(stats.peakRetiredBytes,usage.retiredBytes);stats.peakPendingBytes=std::max(stats.peakPendingBytes,usage.pendingBytes);
    const auto position=world->getComponent<component::MapPosition>(character_)->cell;
    stats.travelCells+=std::hypot(position.x-previousPosition_.x,position.y-previousPosition_.y);
    previousPosition_=position;stats.x=position.x;stats.y=position.y;
    if(stress_ && stats.frames>=stress_->frames()) world->getSingleton<component::WindowHandle>()->windowShouldClose=true;
}
void ServerDemoScenario::sendSnapshot(cgl::ECSCore* world,const cgl::Viewport& viewport,bool replace) {
    const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
    const auto& snapshot=feed_->next(camera,viewport,width_,height_,replace);
    remote_->apply(*world,snapshot,world->getComponent<cgl::component::Sprite>(character_)->clip);
    auto& stats=*world->getSingleton<component::StreamingDiagnostics>();++stats.snapshots;if(replace) ++stats.replacements;
    auto mix=[&](uint64_t value) {stats.fingerprint^=value;stats.fingerprint*=1099511628211ull;};
    mix(snapshot.sequence);
    for(const auto& actor:snapshot.actors) {mix(actor.id);mix(actor.appearance);mix(uint64_t(actor.cell.x));mix(uint64_t(actor.cell.y));}
    lastBatchFrame_=stats.frames;lastBatchCenter_=camera.center;
}
void ServerDemoScenario::clear(ECSCore* world) {
    if(remote_) remote_->clear(*world);
    remote_.reset();feed_.reset();batchClock_={};
    world->removeSingleton<component::StreamingDiagnostics>();stress_.reset();movement_={};lastBatchFrame_=0;
    character_=0;width_=height_=0;
}
}
