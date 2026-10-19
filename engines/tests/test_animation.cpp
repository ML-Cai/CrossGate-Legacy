#include <gtest/gtest.h>
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "engine/ecs.h"
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
