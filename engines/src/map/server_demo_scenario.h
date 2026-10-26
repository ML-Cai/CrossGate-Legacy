#pragma once
#include "map/remote_characters.h"
#include "assets/character_catalog.h"
#include "cgl/assets/map_layout_reader.h"
#include <chrono>
#include "map/streaming_stress.h"
namespace cgl {
// Owns simulated remote entities. The scene clears them before destroying this
// object, then cancels the engine library slot before readers can be replaced.
class ServerDemoScenario {
public:
    explicit ServerDemoScenario(AnimationLibrary& animations):animations_(animations) {}
    // Begin once, poll until true, then update. clear() also cancels an unfinished
    // metadata scan; the owning scene keeps readers alive throughout entry.
    void beginEntry(ECSCore* world,const MapLayout& layout,Entity character);
    bool advanceEntry(ECSCore* world);
    StressMovement movement(ECSCore* world,double x,double y,double seconds);
    void update(ECSCore* world,const Viewport& viewport);
    void clear(ECSCore* world);
private:
    void sendSnapshot(ECSCore* world,const Viewport& viewport,bool replace);
    std::optional<StreamingStress> stress_;
    StressMovement movement_;uint64_t lastBatchFrame_=0;
    MapPoint previousPosition_;
    AnimationLibrary& animations_;
    std::unique_ptr<CharacterCatalogScan> catalog_;
    std::unique_ptr<LocalCharacterFeed> feed_;
    std::unique_ptr<RemoteCharacters> remote_;
    MapPoint lastBatchCenter_;
    std::chrono::steady_clock::time_point batchClock_{};
    uint32_t width_=0,height_=0;
    Entity character_=0;
};
}
