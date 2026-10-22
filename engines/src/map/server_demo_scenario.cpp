#include "map/server_demo_scenario.h"
#include "map/server_demo_refresh.h"
#include "assets/character_catalog.h"
#include "engine/engine_components.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"

namespace cgl {
void ServerDemoScenario::enter(ECSCore* world,const MapLayout& layout,Entity character) {
    width_=layout.width;height_=layout.height;character_=character;
    auto& readers=*world->getSingleton<component::AssetsReaderState>();
    auto catalog=cgl::ReadCharacterCatalog(readers);
    feed_=std::make_unique<cgl::LocalCharacterFeed>(catalog.size());
    remote_=std::make_unique<cgl::RemoteCharacters>(std::move(catalog));
    batchClock_=std::chrono::steady_clock::now();
    const auto& window=*world->getSingleton<cgl::component::WindowHandle>();
    sendSnapshot(world,{0,0,window.width,window.height},true);
}
void ServerDemoScenario::update(ECSCore* world,const Viewport& viewport) {
    if(!remote_ || !viewport.width || !viewport.height) return;
    auto* input=world->getSingleton<component::KeyboardInputState>();
    const auto now=std::chrono::steady_clock::now();
    const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
    ++batchFrames_;
    if(ServerDemoRefreshDue(camera,lastBatchCenter_,viewport,
        std::chrono::duration<double>(now-batchClock_).count(),input->newBatchRequested)) {
        const auto fps=batchFrames_/std::chrono::duration<double>(now-batchClock_).count();
        world->getSingleton<cgl::component::WindowHandle>()->pendingTitle=
            "CGL | FPS " + std::to_string(int(fps)) + " | visible " + std::to_string(remote_->readyVisible) + "/" +
            std::to_string(remote_->visible) + " | prefetch " + std::to_string(remote_->readyOuter) + "/" +
            std::to_string(remote_->outer) + " | WASD move, N randomize, R restart";
        LOGI("Server demo: FPS=" << fps
            << ", visible=" << remote_->readyVisible << "/" << remote_->visible
            << ", outer=" << remote_->readyOuter << "/" << remote_->outer);
        sendSnapshot(world,viewport,input->newBatchRequested);
        batchClock_=now;batchFrames_=0;
    }
    input->newBatchRequested=false;
    auto& readers=*world->getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world->getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    remote_->update(*world,animations_,readers,device,pass,camera,viewport);
}
void ServerDemoScenario::sendSnapshot(cgl::ECSCore* world,const cgl::Viewport& viewport,bool replace) {
    const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
    const auto& snapshot=feed_->next(camera,viewport,width_,height_,replace);
    remote_->apply(*world,snapshot,world->getComponent<cgl::component::Sprite>(character_)->clip);
    lastBatchCenter_=camera.center;
}
void ServerDemoScenario::clear(ECSCore* world) {
    if(remote_) remote_->clear(*world);
    remote_.reset();feed_.reset();batchFrames_=0;batchClock_={};
    character_=0;width_=height_=0;
}
}
