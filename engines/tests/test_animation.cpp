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



TEST(CharacterMovementTest, WASDAndAllFourKeyPairsFollowScreenDirections) {
    // Input x/y, destination MAP delta, native animation direction (0 = screen NW).
    // W, W+D, D, S+D, S, S+A, A, W+A.
    constexpr int inputs[8][5] = {
        {0,-1,1,-1,1},{1,-1,1,0,2},{1,0,1,1,3},{1,1,0,1,4},
        {0,1,-1,1,5},{-1,1,-1,0,6},{-1,0,-1,-1,7},{-1,-1,0,-1,0}};
    for (const auto& input : inputs) {
        SCOPED_TRACE(input[4]);
        MovingCharacter actor;
        actor.update(input[0],input[1], .05);
        EXPECT_TRUE(actor.movement()->stepping);
        EXPECT_EQ(actor.movement()->facing,input[4]);
        EXPECT_NEAR(std::hypot(actor.position().x-50, actor.position().y-50), .2, 1e-10);
        EXPECT_EQ(actor.sprite()->clip,actor.movement()->walk[input[4]]);
        actor.update(0,0,1);
        EXPECT_DOUBLE_EQ(actor.position().x,50+input[2]);
        EXPECT_DOUBLE_EQ(actor.position().y,50+input[3]);
        const auto delta=cgl::MapToWorld({double(input[2]),double(input[3])});
        EXPECT_EQ((delta.x>0)-(delta.x<0),input[0]);
        EXPECT_EQ((delta.y>0)-(delta.y<0),input[1]);
        EXPECT_FALSE(actor.movement()->moving);
        EXPECT_FALSE(actor.movement()->stepping);
        EXPECT_EQ(actor.sprite()->clip,actor.movement()->idle[input[4]]);
    }
}

TEST(CharacterMovementTest, ReleaseCompletesCurrentStepWithoutStartingAnother) {
    MovingCharacter actor;
    actor.update(1,-1,.1); // W+D: screen NE, MAP x+1.
    actor.update(0,0,.05);
    EXPECT_NEAR(actor.position().x,50.6,1e-10);
    EXPECT_TRUE(actor.movement()->moving);
    actor.update(0,0,.1);
    EXPECT_DOUBLE_EQ(actor.position().x,51);
    EXPECT_FALSE(actor.movement()->moving);
    actor.player()->timeMs=250;
    actor.update(0,0,1);
    EXPECT_DOUBLE_EQ(actor.position().x,51);
    EXPECT_EQ(actor.player()->timeMs,250);
}

TEST(CharacterMovementTest, SecondKeyChangesDirectionAtTheNextCellCenter) {
    MovingCharacter actor;
    actor.update(0,-1,.1); // W starts MAP (+1,-1).
    actor.update(-1,-1,.01); // A joins W, but current step keeps facing north.
    EXPECT_EQ(actor.movement()->facing,1);
    actor.update(-1,-1,.3);
    EXPECT_EQ(actor.movement()->facing,0);
    EXPECT_EQ(actor.movement()->targetX,51);
    EXPECT_EQ(actor.movement()->targetY,48);
    actor.update(0,0,1);
    EXPECT_DOUBLE_EQ(actor.position().x,51);
    EXPECT_DOUBLE_EQ(actor.position().y,48);
}

TEST(CharacterMovementTest, TurnWaitsForCellCenterAndPreservesWalkingPhase) {
    MovingCharacter actor;
    actor.update(1,-1,.1);
    actor.player()->timeMs=750;
    auto longer=std::make_shared<cgl::AnimationClip>();
    longer->durationMs=2000; longer->frameCount=8;
    actor.movement()->walk[0]=longer;
    actor.update(-1,-1,.05);
    EXPECT_EQ(actor.movement()->facing,2);
    EXPECT_DOUBLE_EQ(actor.position().y,50);
    actor.update(-1,-1,.15);
    EXPECT_EQ(actor.movement()->facing,0);
    EXPECT_DOUBLE_EQ(actor.position().x,51);
    EXPECT_NEAR(actor.position().y,49.8,1e-10);
    EXPECT_EQ(actor.player()->timeMs,1500);
    EXPECT_EQ(actor.player()->frame,6);
}

TEST(CharacterMovementTest, FramePartitionAndLongHeldInputDoNotDrift) {
    MovingCharacter a,b;
    a.update(1,0,2);
    for(int i=0;i<200;++i) b.update(1,0,.01);
    EXPECT_NEAR(a.position().x,b.position().x,1e-9);
    EXPECT_NEAR(a.position().y,b.position().y,1e-9);
    a.update(0,0,1); b.update(0,0,1);
    EXPECT_DOUBLE_EQ(a.position().x,56);
    EXPECT_DOUBLE_EQ(a.position().y,56);
    EXPECT_DOUBLE_EQ(a.position().x,b.position().x);
}

TEST(CharacterMovementTest, BoundaryRejectsWholeStepWithoutSlidingOrLeavingMap) {
    MovingCharacter actor;
    actor.position()={99,50};
    actor.update(1,0,1);
    EXPECT_FALSE(actor.movement()->moving);
    EXPECT_DOUBLE_EQ(actor.position().y,50);
    actor.update(1,1,1000);
    EXPECT_DOUBLE_EQ(actor.position().x,99);
    EXPECT_DOUBLE_EQ(actor.position().y,99);
    EXPECT_FALSE(actor.movement()->moving);
    actor.update(-1,1,.25);
    actor.update(0,0,.1);
    EXPECT_DOUBLE_EQ(actor.position().x,98);
}

TEST(CharacterMovementTest, InvalidTimeInputAndSpeedDoNotAdvanceAnActiveStep) {
    MovingCharacter actor;
    actor.update(1,0,.1);
    const auto x=actor.position().x;
    actor.update(1,0,std::numeric_limits<double>::quiet_NaN());
    actor.update(1,0,-1);
    actor.update(std::numeric_limits<double>::infinity(),0);
    actor.movement()->cellsPerSecond=0;
    actor.update(1,0,1);
    EXPECT_DOUBLE_EQ(actor.position().x,x);
}

TEST(CharacterMovementTest, MissingClipDoesNotStartAStep) {
    MovingCharacter actor;
    actor.movement()->walk[3].reset();
    actor.update(1,0);
    EXPECT_DOUBLE_EQ(actor.position().x,50);
    EXPECT_FALSE(actor.movement()->stepping);
}

namespace { struct JoinA { int value=0; }; struct JoinB { int value=0; }; struct JoinC {}; }
TEST(engine_ecs, DenseJoinMatchesSparseIntersectionAfterSwapRemoval) {
    cgl::ECSCore world;
    std::vector<cgl::Entity> ids;
    for(int i=0;i<1000;++i) {
        auto e=world.createEntity();ids.push_back(e);world.addComponent<JoinA>(e,i);
        if(i%7==0) world.addComponent<JoinB>(e,i);
        if(i%5==0) world.addComponent<JoinC>(e);
    }
    world.destroyEntity(ids[35]);world.destroyEntity(ids[70]);
    std::unordered_set<cgl::Entity> seen;
    world.forEach<JoinC,JoinA,JoinB>([&](auto e,auto*,auto* a,auto* b){
        EXPECT_EQ(a->value,b->value);EXPECT_EQ(a->value%35,0);
        EXPECT_TRUE(seen.insert(e).second);++a->value;
    });
    for(int i=0;i<1000;++i) {
        const bool match=i%35==0 && i!=35 && i!=70;
        EXPECT_EQ(seen.count(ids[i]),match?1:0);
        if(match) EXPECT_EQ(world.getComponent<JoinA>(ids[i])->value,i+1);
    }
}
TEST(engine_ecs, DenseJoinHandlesMissingEmptyAndDuplicateStores) {
    cgl::ECSCore world;auto e=world.createEntity();world.addComponent<JoinA>(e,3);
    int count=0;
    world.forEach<JoinA,JoinB>([&](auto,auto*,auto*){++count;});EXPECT_EQ(count,0);
    world.forEach<JoinA,JoinA>([&](auto entity,auto* a,auto* b){EXPECT_EQ(entity,e);EXPECT_EQ(a,b);++count;});
    EXPECT_EQ(count,1);
    world.forEach<>([&](auto entity){EXPECT_EQ(entity,e);++count;});EXPECT_EQ(count,2);
    world.addComponent<JoinB>(e,4);world.destroyEntity(e);
    world.forEach<JoinA,JoinB>([&](auto,auto*,auto*){++count;});EXPECT_EQ(count,2);
}
