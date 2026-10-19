#pragma once
#include "assets/animation_library.h"

namespace cgl::component {
struct Transform2D { float x = 0, y = 0, scale = 1; };
struct Sprite { std::shared_ptr<const AnimationClip> clip; };
struct AnimationPlayer {
    double timeMs = 0;
    double speed = 1;
    uint32_t frame = 0;
    bool playing = true;
    bool looping = true;
};
}
