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
    double cellsPerSecond = 4;
    int32_t cellX = 0, cellY = 0, targetX = 0, targetY = 0;
    double progress = 0;
    bool initialized = false, stepping = false;
    bool moving = false;
};
}

namespace cgl {
// Input directions are screen-relative: W/up, A/left, S/down, D/right.
// Quantize the inverse isometric direction to an integer neighboring MAP cell.
// Rendering interpolates; release or a turn takes effect at the next cell center.
inline void UpdateCharacterMovement(ECSCore& world, double horizontal, double vertical,
    double seconds, uint32_t width, uint32_t height) {
    if (!width || !height || !std::isfinite(seconds) || seconds <= 0 ||
        !std::isfinite(horizontal) || !std::isfinite(vertical)) return;
    const int horizontalSign = (horizontal > 0) - (horizontal < 0);
    const int verticalSign = (vertical > 0) - (vertical < 0);
    const int mapX = horizontalSign - verticalSign;
    const int mapY = horizontalSign + verticalSign;
    const int dx = (mapX > 0) - (mapX < 0);
    const int dy = (mapY > 0) - (mapY < 0);
    world.forEach<component::MapPosition, component::CharacterMovement,
        component::Sprite, component::AnimationPlayer>(
        [&](Entity, auto* position, auto* movement, auto* sprite, auto* player) {
            if (!std::isfinite(movement->cellsPerSecond) || movement->cellsPerSecond <= 0) return;
            if (!movement->initialized) {
                if (!std::isfinite(position->cell.x) || !std::isfinite(position->cell.y)) return;
                movement->cellX = int32_t(std::round(std::clamp(position->cell.x, 0.0, double(width-1))));
                movement->cellY = int32_t(std::round(std::clamp(position->cell.y, 0.0, double(height-1))));
                movement->initialized = true;
                position->cell = {double(movement->cellX), double(movement->cellY)};
            }
            auto canStep = [&] {
                const int64_t x = int64_t(movement->cellX) + dx, y = int64_t(movement->cellY) + dy;
                return (dx || dy) && x >= 0 && y >= 0 && x < width && y < height;
            };
            auto selectClip = [&](bool moving, uint32_t facing) {
                if (facing >= 8) return false;
                const auto& clip = moving ? movement->walk[facing] : movement->idle[facing];
                if (!clip || !clip->durationMs || !clip->frameCount) return false;
                if (sprite->clip != clip) {
                    const double phase = moving && movement->moving && sprite->clip &&
                        sprite->clip->durationMs && std::isfinite(player->timeMs)
                        ? std::fmod(std::max(0.0, player->timeMs), double(sprite->clip->durationMs)) /
                            sprite->clip->durationMs : 0;
                    player->timeMs = phase * clip->durationMs;
                    player->frame = uint32_t(phase * clip->frameCount);
                    sprite->clip = clip;
                    player->playing = player->looping = true;
                }
                movement->facing = facing;
                movement->moving = moving;
                return true;
            };
            double remaining = seconds;
            while (remaining > 0) {
                if (!movement->stepping) {
                    if (!canStep()) break;
                    const auto delta = MapToWorld({double(dx), double(dy)});
                    const auto facing = uint32_t((int(std::lround(std::atan2(delta.x, -delta.y) /
                        (std::numbers::pi / 4))) + 8) % 8);
                    if (!selectClip(true, facing)) break;
                    movement->targetX = movement->cellX + dx;
                    movement->targetY = movement->cellY + dy;
                    movement->progress = 0;
                    movement->stepping = true;
                }
                const double duration = std::hypot(double(movement->targetX - movement->cellX),
                    double(movement->targetY - movement->cellY)) / movement->cellsPerSecond;
                const double needed = (1 - movement->progress) * duration;
                if (remaining < needed) {
                    movement->progress += remaining / duration;
                    remaining = 0;
                } else {
                    remaining -= needed;
                    movement->cellX = movement->targetX;
                    movement->cellY = movement->targetY;
                    movement->progress = 0;
                    movement->stepping = false;
                }
                position->cell = {movement->cellX + (movement->targetX - movement->cellX) * movement->progress,
                                  movement->cellY + (movement->targetY - movement->cellY) * movement->progress};
            }
            if (!movement->stepping && !canStep()) selectClip(false, movement->facing);
        });
}
}
