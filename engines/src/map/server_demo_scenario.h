#pragma once
#include "map/remote_characters.h"
#include "cgl/assets/map_layout_reader.h"
#include <chrono>
namespace cgl {
// Owns simulated remote entities. The scene clears them before destroying this
// object, then cancels the engine library slot before readers can be replaced.
class ServerDemoScenario {
public:
    explicit ServerDemoScenario(AnimationLibrary& animations):animations_(animations) {}
    void enter(ECSCore* world,const MapLayout& layout,Entity character);
    void update(ECSCore* world,const Viewport& viewport);
    void clear(ECSCore* world);
private:
    void sendSnapshot(ECSCore* world,const Viewport& viewport,bool replace);
    AnimationLibrary& animations_;
    std::unique_ptr<LocalCharacterFeed> feed_;
    std::unique_ptr<RemoteCharacters> remote_;
    MapPoint lastBatchCenter_;
    std::chrono::steady_clock::time_point batchClock_{};
    uint64_t batchFrames_=0;
    uint32_t width_=0,height_=0;
    Entity character_=0;
};
}
