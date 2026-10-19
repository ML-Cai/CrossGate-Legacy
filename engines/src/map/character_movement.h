#pragma once

#include "animation/animation_components.h"
#include "map/map_render_data.h"
#include "engine/ecs.h"
#include <array>
#include <numbers>

namespace cgl::component {
// Clips are immutable engine-cached assets; facing and playback belong to entities.
struct CharacterMovement {
    std::array<std::shared_ptr<const AnimationClip>, 8> idle, walk;
    uint32_t facing = 0;
    double pixelsPerSecond = 100;
    bool moving = false;
};
}

namespace cgl {
// Input directions are screen/world-relative (W is up), independent of MAP axes.
inline void UpdateCharacterMovement(ECSCore& world, double horizontal, double vertical,
    double seconds, uint32_t width, uint32_t height) {
    if (!std::isfinite(seconds) || seconds <= 0 ||
        !std::isfinite(horizontal) || !std::isfinite(vertical)) return;
    world.forEach<component::MapPosition, component::CharacterMovement,
        component::Sprite, component::AnimationPlayer>(
        [&](Entity, auto* position, auto* movement, auto* sprite, auto* player) {
            auto next = position->cell;
            MoveMapPoint(next, horizontal, vertical, seconds, movement->pixelsPerSecond, width, height);
            const auto before = MapToWorld(position->cell), after = MapToWorld(next);
            const double dx = after.x - before.x, dy = after.y - before.y;
            const bool moving = std::hypot(dx, dy) > 1e-9;
            const uint32_t facing = moving
                ? uint32_t((int(std::lround(std::atan2(dx, -dy) / (std::numbers::pi / 4))) + 8) % 8)
                : movement->facing;
            if (facing >= 8) return;
            const auto& clip = moving ? movement->walk[facing] : movement->idle[facing];
            if (!clip || !clip->durationMs || !clip->frameCount) return;
            if (sprite->clip != clip) {
                // Preserve gait phase when turning; start a new cycle when stopping/starting.
                const double phase = moving && movement->moving && sprite->clip &&
                    sprite->clip->durationMs && std::isfinite(player->timeMs)
                    ? std::fmod(std::max(0.0, player->timeMs), double(sprite->clip->durationMs)) /
                        sprite->clip->durationMs : 0;
                player->timeMs = phase * clip->durationMs;
                player->frame = uint32_t(phase * clip->frameCount);
                sprite->clip = clip;
                player->playing = player->looping = true;
            }
            position->cell = next;
            movement->facing = facing;
            movement->moving = moving;
        });
}
}
