#pragma once
#include "map/local_character_feed.h"
#include "map/character_movement.h"
#include "assets/animation_library.h"
#include "animation/animation_components.h"
#include "engine/ecs.h"
#include "cgl/trace/logger.h"
#include <set>
#include <chrono>
#include "map/appearance_scheduler.h"

namespace cgl {
namespace component { struct RemoteCharacter { uint64_t serverId; size_t appearance; }; }
// Scene-owned bridge from complete snapshots to ECS. It owns entity identities,
// never GPU resources; engine ready/retired ownership covers submitted GPU work.
class RemoteCharacters {
public:
    explicit RemoteCharacters(std::vector<AnimationRequest> catalog):catalog_(std::move(catalog)) {}
    void clear(ECSCore& world) {
        for(const auto& [id,entity]:entities_) world.destroyEntity(entity);
        entities_.clear(); placeholder_.reset(); visible=readyVisible=outer=readyOuter=0; sequence_=0; active_.reset(); failed_.clear();scheduler_.clear();demands_.clear();
    }
    void apply(ECSCore& world,const CharacterSnapshot& packet,std::shared_ptr<const AnimationClip> placeholder) {
        if(packet.sequence<=sequence_) return;
        if(packet.actors.size()>1024) throw std::invalid_argument("Snapshot too large");
        std::set<uint64_t> ids;
        for(const auto& actor:packet.actors)
            if(!actor.id || !ids.insert(actor.id).second || actor.appearance>=catalog_.size() ||
               !std::isfinite(actor.cell.x) || !std::isfinite(actor.cell.y))
                throw std::invalid_argument("Invalid character snapshot");
        placeholder_=placeholder;
        for(auto it=entities_.begin();it!=entities_.end();) {
            if(!ids.contains(it->first)) {world.destroyEntity(it->second);it=entities_.erase(it);} else ++it;
        }
        for(const auto& actor:packet.actors) {
            auto found=entities_.find(actor.id);
            if(found==entities_.end()) {
                const auto entity=world.createEntity();
                try {entities_.emplace(actor.id,entity);} catch(...) {world.destroyEntity(entity);throw;}
                world.addComponent<component::MapPosition>(entity,actor.cell);
                world.addComponent<component::Sprite>(entity,placeholder);
                world.addComponent<component::AnimationPlayer>(entity,double(actor.id%1000));
                world.addComponent<component::RemoteCharacter>(entity,actor.id,actor.appearance);
            } else {
                world.getComponent<component::MapPosition>(found->second)->cell=actor.cell;
                auto* tag=world.getComponent<component::RemoteCharacter>(found->second);
                if(tag->appearance!=actor.appearance) {
                    world.getComponent<component::Sprite>(found->second)->clip=placeholder;
                    *world.getComponent<component::AnimationPlayer>(found->second)={};
                    tag->appearance=actor.appearance;
                }
            }
        }
        sequence_=packet.sequence;
    }
    void update(ECSCore& world,AnimationLibrary& library,component::AssetsReaderState& readers,
                graphics::IDevice* device,graphics::IRenderPass* pass,
                const component::MapCamera& camera,const Viewport& viewport) {
        auto& demands=demands_;demands.clear();demands.reserve(entities_.size());
        visible=readyVisible=outer=readyOuter=0;
        for(const auto& [id,entity]:entities_) {
            auto* tag=world.getComponent<component::RemoteCharacter>(entity);
            auto* sprite=world.getComponent<component::Sprite>(entity);
            const auto cell=world.getComponent<component::MapPosition>(entity)->cell;
            const int region=LocalCharacterFeed::region(cell,camera,viewport);
            // Fully off-screen actors retain data, not texture pins. Prefetched clips
            // remain cache-owned and can retire under pressure. Shared visible
            // appearances stay protected by their visible Sprite owners.
            const auto ready=library.findReady(catalog_[tag->appearance]);
            const auto p=MapToScreen(cell,camera,viewport);
            // Match renderer bounds: an off-screen foot can still have visible pixels.
            const bool intersects=ready && MapImageVisible(
                {p.x+ready->offsetX*camera.scale,p.y+ready->offsetY*camera.scale},
                ready->width,ready->height,camera.scale,viewport);
            const auto presented=ready && (region==0 || intersects) ? ready : placeholder_;
            if(sprite->clip!=presented) {
                sprite->clip=presented; *world.getComponent<component::AnimationPlayer>(entity)={};
            }
            if(region==0) {++visible;if(ready) ++readyVisible;}
            if(region==1) {++outer;if(ready) ++readyOuter;}
            if(ready || region<0 || failed_.contains(tag->appearance)) continue;
            const double distance=std::hypot(p.x-viewport.width*.5,p.y-viewport.height*.5);
            demands.push_back({tag->appearance,region,distance});
        }
        const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        const auto candidate=scheduler_.update(demands,now);
        // Only stale CPU work or prefetch displaced by visible demand is cancelled.
        // Keep same-region active work stable; nearer actors must not cause thrash.
        if(active_ && scheduler_.shouldSupersede(*active_,candidate) && library.discardUnsubmitted())
            active_.reset();
        if(!active_) active_=candidate;
        if(!active_) return;
        try {
            if(library.acquireProgressive(catalog_[*active_],readers,device,pass)) active_.reset();
        } catch(const std::exception& error) {
            LOGE("Character appearance failed: " << error.what());
            failed_.insert(*active_);library.cancelPending();active_.reset();
        }
    }
    size_t visible=0,readyVisible=0,outer=0,readyOuter=0;
private:
    std::shared_ptr<const AnimationClip> placeholder_;
    std::vector<AppearanceScheduler::Demand> demands_;
    AppearanceScheduler scheduler_;
    std::vector<AnimationRequest> catalog_;
    std::map<uint64_t,Entity> entities_;
    std::optional<size_t> active_;
    std::set<size_t> failed_;
    uint64_t sequence_=0;
};
}
