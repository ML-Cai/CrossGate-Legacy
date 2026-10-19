#include "animation/animation_system.h"
#include "animation/animation_components.h"
#include "engine/ecs.h"
#include <algorithm>
#include <cmath>

void cgl::UpdateAnimations(ECSCore& world, double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) return;
    world.forEach<component::Sprite, component::AnimationPlayer>(
        [deltaSeconds](Entity, auto* sprite, auto* player) {
            const auto& clip = sprite->clip;
            if (!clip || !clip->frameCount || !clip->durationMs ||
                !std::isfinite(player->speed) || !std::isfinite(player->timeMs)) return;
            const double duration = clip->durationMs;
            if (player->playing) player->timeMs += deltaSeconds * 1000 * player->speed;
            if (!std::isfinite(player->timeMs)) { player->timeMs = 0; player->frame = 0; return; }
            if (player->looping) {
                player->timeMs = std::fmod(player->timeMs, duration);
                if (player->timeMs < 0) player->timeMs += duration;
            } else {
                player->timeMs = std::clamp(player->timeMs, 0.0, duration);
                if ((player->speed >= 0 && player->timeMs >= duration) ||
                    (player->speed < 0 && player->timeMs <= 0)) player->playing = false;
            }
            player->frame = std::min(clip->frameCount - 1,
                uint32_t(player->timeMs / duration * clip->frameCount));
        });
}
