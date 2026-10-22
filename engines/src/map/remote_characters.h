#pragma once
#include "map/local_character_feed.h"
#include "map/character_movement.h"
#include "assets/animation_library.h"
#include "animation/animation_components.h"
#include "engine/ecs.h"
#include "cgl/trace/logger.h"
#include <set>

namespace cgl {
namespace component { struct RemoteCharacter { uint64_t serverId; size_t appearance; }; }
// Scene-owned bridge from complete snapshots to ECS. It owns entity identities,
// never GPU resources; the engine library retains ready clips until safe teardown.
class RemoteCharacters {
public:
    explicit RemoteCharacters(std::vector<AnimationRequest> catalog):catalog_(std::move(catalog)) {}
    void clear(ECSCore& world) {
        for(const auto& [id,entity]:entities_) world.destroyEntity(entity);
        entities_.clear(); sequence_=0; active_.reset(); failed_.clear();
    }
    void apply(ECSCore& world,const CharacterSnapshot& packet,std::shared_ptr<const AnimationClip> placeholder) {
        if(packet.sequence<=sequence_) return;
        if(packet.actors.size()>1024) throw std::invalid_argument("Snapshot too large");
        std::set<uint64_t> ids;
        for(const auto& actor:packet.actors)
            if(!actor.id || !ids.insert(actor.id).second || actor.appearance>=catalog_.size() ||
               !std::isfinite(actor.cell.x) || !std::isfinite(actor.cell.y))
                throw std::invalid_argument("Invalid character snapshot");
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
        int bestRegion=2; double bestDistance=0; std::optional<size_t> candidate;
        visible=readyVisible=outer=readyOuter=0;
        for(const auto& [id,entity]:entities_) {
            auto* tag=world.getComponent<component::RemoteCharacter>(entity);
            auto* sprite=world.getComponent<component::Sprite>(entity);
            const auto ready=library.findReady(catalog_[tag->appearance]);
            if(ready && sprite->clip!=ready) {
                sprite->clip=ready; *world.getComponent<component::AnimationPlayer>(entity)={};
            }
            const auto cell=world.getComponent<component::MapPosition>(entity)->cell;
            const int region=LocalCharacterFeed::region(cell,camera,viewport);
            if(region==0) {++visible;if(ready) ++readyVisible;}
            if(region==1) {++outer;if(ready) ++readyOuter;}
            if(ready || region<0 || failed_.contains(tag->appearance)) continue;
            const auto p=MapToScreen(cell,camera,viewport);
            const double distance=std::hypot(p.x-viewport.width*.5,p.y-viewport.height*.5);
            if(region<bestRegion || (region==bestRegion && distance<bestDistance)) {
                bestRegion=region;bestDistance=distance;candidate=tag->appearance;
            }
        }
        // Finish an in-flight clip even if its actor left. Cancelling a submitted
        // chunk on every snapshot could stall the frame. No unbounded FIFO exists.
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
    std::vector<AnimationRequest> catalog_;
    std::map<uint64_t,Entity> entities_;
    std::optional<size_t> active_;
    std::set<size_t> failed_;
    uint64_t sequence_=0;
};
}
