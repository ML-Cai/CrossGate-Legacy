#include <gtest/gtest.h>
#include "scene/scene_animation_load.h"
#include "assets/animation_library.h"
#include <limits>
TEST(SceneAnimationLoad, AdmitsWholeWorkingSetBeforeAcquiringAndRejectsInvalidPlans) {
    cgl::SceneAnimationLoad load;
    const cgl::AnimationRequest a{1,0,0},b{2,0,0};
    EXPECT_THROW(load.begin({a,b},100,[](const auto&){return uint64_t(60);}),std::runtime_error);
    EXPECT_EQ(load.total(),0);EXPECT_FALSE(load.ready());EXPECT_EQ(load.requiredBytes(),0);
    EXPECT_THROW(load.begin({a,a},100,[](const auto&){return uint64_t(1);}),std::invalid_argument);
    EXPECT_THROW(load.begin({},100,[](const auto&){return uint64_t(1);}),std::invalid_argument);
    EXPECT_THROW(load.begin(std::vector<cgl::AnimationRequest>(17,a),100,[](const auto&){return uint64_t(1);}),std::invalid_argument);
    EXPECT_THROW(load.begin({a},100,[](const auto&){return uint64_t(0);}),std::runtime_error);
    EXPECT_THROW(load.begin({a,b},std::numeric_limits<uint64_t>::max(),[](const auto& r) {
        return r.serial==1?std::numeric_limits<uint64_t>::max()-1:2;
    }),std::runtime_error);
}
TEST(SceneAnimationLoad, RetainsPartialOwnershipWithoutReacquiringOrReplanningAndResetReleasesIt) {
    cgl::SceneAnimationLoad load;int planned=0,acquired=0;
    const cgl::AnimationRequest a{1,0,0},b{2,0,0};
    load.begin({a,b},100,[&](const auto&){++planned;return uint64_t(50);});
    auto first=std::make_shared<cgl::AnimationClip>(),second=std::make_shared<cgl::AnimationClip>();
    std::weak_ptr<const cgl::AnimationClip> weak=first;
    EXPECT_FALSE(load.advance([&](const auto& r)->std::shared_ptr<const cgl::AnimationClip> {
        ++acquired;return r.serial==1?first:nullptr;
    }));
    EXPECT_EQ(load.completed(),1);EXPECT_EQ(load.requiredBytes(),100);EXPECT_EQ(planned,2);EXPECT_EQ(acquired,2);
    first.reset();EXPECT_FALSE(weak.expired());EXPECT_THROW(load.clip(1),std::out_of_range);
    EXPECT_TRUE(load.advance([&](const auto& r)->std::shared_ptr<const cgl::AnimationClip> {
        ++acquired;EXPECT_EQ(r.serial,2);return second;
    }));
    EXPECT_TRUE(load.ready());EXPECT_EQ(load.completed(),2);EXPECT_EQ(planned,2);EXPECT_EQ(acquired,3);
    EXPECT_TRUE(load.advance([](const auto&)->std::shared_ptr<const cgl::AnimationClip>{ADD_FAILURE();return nullptr;}));
    load.reset();EXPECT_TRUE(weak.expired());EXPECT_EQ(load.total(),0);EXPECT_FALSE(load.ready());
}
TEST(SceneAnimationLoad, FailedAcquisitionKeepsTrackedPartialOwnersUntilCancellation) {
    cgl::SceneAnimationLoad load;load.begin({{1,0,0},{2,0,0}},100,[](const auto&){return uint64_t(10);});
    std::weak_ptr<const cgl::AnimationClip> weak;
    EXPECT_THROW(load.advance([&](const auto& r)->std::shared_ptr<const cgl::AnimationClip> {
        if(r.serial==2) throw std::runtime_error("Bad asset");
        auto clip=std::make_shared<cgl::AnimationClip>();weak=clip;return clip;
    }),std::runtime_error);
    EXPECT_EQ(load.completed(),1);EXPECT_FALSE(weak.expired());load.reset();EXPECT_TRUE(weak.expired());
    EXPECT_THROW(load.begin({{1,0,0}},100,[](const auto&)->uint64_t{throw std::runtime_error("Bad metadata");}),std::runtime_error);
    EXPECT_EQ(load.total(),0);EXPECT_EQ(load.completed(),0);
}
