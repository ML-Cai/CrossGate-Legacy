#pragma once
#include "map/character_movement.h"
#include "animation/animation_system.h"
namespace cgl {
// Explicit inputs: no window, scene transitions or device access. Playback and
// camera follow are advanced only by the update stage, never by rendering.
inline void UpdateMapWorld(ECSCore& world,Entity followed,uint32_t width,uint32_t height,
    double horizontal,double vertical,double seconds) {
    UpdateCharacterMovement(world,horizontal,vertical,seconds,width,height);
    if(auto* camera=world.getSingleton<component::MapCamera>())
        if(const auto* position=world.getComponent<component::MapPosition>(followed)) camera->center=position->cell;
    UpdateAnimations(world,seconds);
}
}
