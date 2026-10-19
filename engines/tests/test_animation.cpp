#include <gtest/gtest.h>
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "engine/ecs.h"
#include "map/character_movement.h"
#include <limits>

namespace {
std::shared_ptr<const cgl::AnimationClip> clip() {
    auto asset = std::make_shared<cgl::AnimationClip>();
    asset->durationMs = 1000;
    asset->frameCount = 10;
    return asset;
}
cgl::Entity character(cgl::ECSCore& world, std::shared_ptr<const cgl::AnimationClip> asset, double offset = 0) {
    const auto e = world.createEntity();
    world.addComponent<cgl::component::Sprite>(e, std::move(asset));
    world.addComponent<cgl::component::AnimationPlayer>(e, offset);
    return e;
}
}

TEST(engine_animation, SharedClipIndependentPlayback) {
    cgl::ECSCore world;
    auto asset = clip();
    const auto a = character(world, asset);
    const auto b = character(world, asset, 500);
    cgl::UpdateAnimations(world, .25);
    EXPECT_EQ(world.getComponent<cgl::component::Sprite>(a)->clip,
              world.getComponent<cgl::component::Sprite>(b)->clip);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(a)->frame, 2);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(b)->frame, 7);
    world.getComponent<cgl::component::AnimationPlayer>(a)->playing = false;
    cgl::UpdateAnimations(world, .4);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(a)->frame, 2);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(b)->frame, 1);
}

TEST(engine_animation, LargeDeltaLoopAndNonLoopCompletion) {
    cgl::ECSCore world;
    const auto e = character(world, clip());
    cgl::UpdateAnimations(world, 2.35);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(e)->frame, 3);
    auto* player = world.getComponent<cgl::component::AnimationPlayer>(e);
    player->looping = false;
    cgl::UpdateAnimations(world, 2);
    EXPECT_EQ(player->frame, 9);
    EXPECT_FALSE(player->playing);
}

TEST(engine_animation, ReversePlaybackAndInvalidDelta) {
    cgl::ECSCore world;
    const auto e = character(world, clip(), 100);
    auto* player = world.getComponent<cgl::component::AnimationPlayer>(e);
    player->speed = -1;
    cgl::UpdateAnimations(world, .2);
    EXPECT_EQ(player->frame, 9);
    cgl::UpdateAnimations(world, std::numeric_limits<double>::quiet_NaN());
    cgl::UpdateAnimations(world, -.3);
    EXPECT_EQ(player->frame, 9);
}

TEST(engine_animation, RemovingOneEntityPreservesOtherPlaybackAndAsset) {
    cgl::ECSCore world;
    auto asset = clip();
    std::weak_ptr<const cgl::AnimationClip> weak = asset;
    const auto a = character(world, asset);
    const auto b = character(world, asset, 500);
    asset.reset();
    world.destroyEntity(a); // Exercises dense-storage swap removal.
    EXPECT_FALSE(weak.expired());
    cgl::UpdateAnimations(world, .1);
    EXPECT_EQ(world.getComponent<cgl::component::AnimationPlayer>(b)->frame, 6);
    world.destroyEntity(b);
    EXPECT_TRUE(weak.expired());
    unsigned count = 0;
    world.forEach<cgl::component::Sprite>([&](auto, auto*) { ++count; });
    EXPECT_EQ(count, 0);
}

TEST(engine_ecs, DestroyedSingletonCanBeRecreated) {
    cgl::ECSCore world;
    const auto e = world.addSingleton<int>(7);
    world.destroyEntity(e);
    EXPECT_EQ(world.getSingleton<int>(), nullptr);
    world.addSingleton<int>(9);
    ASSERT_NE(world.getSingleton<int>(), nullptr);
    EXPECT_EQ(*world.getSingleton<int>(), 9);
    world.removeSingleton<int>();
    EXPECT_EQ(world.getSingleton<int>(), nullptr);
}


namespace {
struct MovingCharacter {
    cgl::ECSCore world;
    cgl::Entity entity;
    MovingCharacter() {
        entity = character(world, clip());
        world.addComponent<cgl::component::MapPosition>(entity, cgl::MapPoint{50, 50});
        cgl::component::CharacterMovement movement;
        for (unsigned d = 0; d < 8; ++d) {
            movement.idle[d] = clip();
            movement.walk[d] = clip();
        }
        world.getComponent<cgl::component::Sprite>(entity)->clip = movement.idle[0];
        world.addComponent<cgl::component::CharacterMovement>(entity, std::move(movement));
    }
    auto* movement() { return world.getComponent<cgl::component::CharacterMovement>(entity); }
    auto* player() { return world.getComponent<cgl::component::AnimationPlayer>(entity); }
    auto* sprite() { return world.getComponent<cgl::component::Sprite>(entity); }
    auto& position() { return world.getComponent<cgl::component::MapPosition>(entity)->cell; }
    void update(double x, double y, double seconds = .1) {
        cgl::UpdateCharacterMovement(world, x, y, seconds, 100, 100);
    }
};
}

TEST(CharacterMovementTest, EightScreenDirectionsAndEqualSpeed) {
    constexpr int inputs[8][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    for (unsigned d = 0; d < 8; ++d) {
        MovingCharacter actor;
        const auto start = cgl::MapToWorld(actor.position());
        actor.update(inputs[d][0], inputs[d][1]);
        const auto end = cgl::MapToWorld(actor.position());
        EXPECT_NEAR(std::hypot(end.x-start.x, end.y-start.y), 10, 1e-9);
        EXPECT_EQ(actor.movement()->facing, d);
        EXPECT_TRUE(actor.movement()->moving);
        EXPECT_EQ(actor.sprite()->clip, actor.movement()->walk[d]);
        EXPECT_NEAR(end.x-start.x, inputs[d][0] * 10 / std::hypot(inputs[d][0],inputs[d][1]), 1e-9);
        EXPECT_NEAR(end.y-start.y, inputs[d][1] * 10 / std::hypot(inputs[d][0],inputs[d][1]), 1e-9);
    }
}

TEST(CharacterMovementTest, StopKeepsFacingAndRestartsIdleOnlyOnce) {
    MovingCharacter actor;
    actor.update(1,0);
    actor.player()->timeMs = 500;
    actor.update(1,0);
    EXPECT_EQ(actor.player()->timeMs, 500);
    actor.update(0,0);
    EXPECT_FALSE(actor.movement()->moving);
    EXPECT_EQ(actor.movement()->facing, 2);
    EXPECT_EQ(actor.sprite()->clip, actor.movement()->idle[2]);
    EXPECT_EQ(actor.player()->timeMs, 0);
    actor.player()->timeMs = 250;
    actor.update(0,0);
    EXPECT_EQ(actor.player()->timeMs, 250);
    actor.update(0,-1);
    EXPECT_EQ(actor.player()->timeMs, 0);
}

TEST(CharacterMovementTest, TurningPreservesNormalizedGaitPhase) {
    MovingCharacter actor;
    actor.update(1,0);
    actor.player()->timeMs = 750;
    auto longer = std::make_shared<cgl::AnimationClip>();
    longer->durationMs = 2000; longer->frameCount = 8;
    actor.movement()->walk[0] = longer;
    actor.update(0,-1);
    EXPECT_EQ(actor.player()->timeMs, 1500);
    EXPECT_EQ(actor.player()->frame, 6);
    EXPECT_EQ(actor.sprite()->clip, longer);
}

TEST(CharacterMovementTest, BoundsStopAndInvalidInputDoesNotMove) {
    MovingCharacter actor;
    actor.position() = {99,99};
    actor.update(1,0);
    EXPECT_FALSE(actor.movement()->moving);
    EXPECT_DOUBLE_EQ(actor.position().x,99);
    EXPECT_DOUBLE_EQ(actor.position().y,99);
    actor.update(-1,0, std::numeric_limits<double>::quiet_NaN());
    actor.update(-1,0, -1);
    actor.update(std::numeric_limits<double>::infinity(),0);
    EXPECT_DOUBLE_EQ(actor.position().x,99);
    actor.update(-1,0);
    EXPECT_TRUE(actor.movement()->moving);
    EXPECT_LT(actor.position().x,99);
    EXPECT_LT(actor.position().y,99);
}

TEST(CharacterMovementTest, MissingClipDoesNotCommitMovement) {
    MovingCharacter actor;
    actor.movement()->walk[2].reset();
    actor.update(1,0);
    EXPECT_DOUBLE_EQ(actor.position().x,50);
    EXPECT_DOUBLE_EQ(actor.position().y,50);
    EXPECT_FALSE(actor.movement()->moving);
}
