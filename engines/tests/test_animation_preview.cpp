#include <gtest/gtest.h>
#include "scene/animation_scene/animation_preview.h"
#include "cgl/settings/settings.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

TEST(AnimationPreview, EveryVersionPreservesTheRequestedIdentity) {
    for(uint32_t version=0;version<uint32_t(cgl::CrossGateVersion::COUNT);++version) {
        const auto request=cgl::AnimationPreviewRequest(version,110300,7,1);
        EXPECT_EQ(uint32_t(request.version),version);EXPECT_EQ(request.serial,110300);
        EXPECT_EQ(request.direction,7);EXPECT_EQ(request.motion,1);
    }
}
TEST(AnimationPreview, InvalidSelectionFailsWithoutReadersOrDevice) {
    EXPECT_THROW(cgl::AnimationPreviewRequest(uint32_t(cgl::CrossGateVersion::COUNT),1,0,0),std::runtime_error);
    EXPECT_THROW(cgl::AnimationPreviewRequest(std::numeric_limits<uint32_t>::max(),1,0,0),std::runtime_error);
    EXPECT_THROW(cgl::AnimationPreviewRequest(0,1,8,0),std::runtime_error);
    EXPECT_THROW(cgl::AnimationPreviewRequest(0,1,0,uint32_t(cgl::MotionTypes::COUNT)),std::runtime_error);
}
TEST(AnimationPreview, ExactFramePhaseSurvivesPlaybackFrameCalculation) {
    EXPECT_EQ(cgl::AnimationPreviewStartTime(0,7,101),0);
    for(const uint32_t count : {1u,4u,7u,45u,65536u}) {
        for(uint32_t frame=0;frame<count;++frame) {
            const auto time=cgl::AnimationPreviewStartTime(frame,count,101);
            EXPECT_EQ(uint32_t(time/101*count),frame);
            EXPECT_GE(time,0);EXPECT_LT(time,101);
        }
    }
    EXPECT_THROW(cgl::AnimationPreviewStartTime(4,4,100),std::runtime_error);
    EXPECT_THROW(cgl::AnimationPreviewStartTime(0,0,100),std::runtime_error);
    EXPECT_THROW(cgl::AnimationPreviewStartTime(0,4,0),std::runtime_error);
}
TEST(AnimationPreview, IniSelectionAndLegacyDefaults) {
    const cgl::Settings defaults{};
    EXPECT_EQ(defaults.animation.Version,0);EXPECT_FALSE(defaults.animation.Paused);EXPECT_EQ(defaults.animation.StartFrame,0);
    const auto path=std::filesystem::temp_directory_path()/
        ("cgl-preview-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
    {std::ofstream file(path);file<<"[Animation]\nVersion=3\nSerialNum=110300\nDirection=7\nMotion=1\nPaused=true\nStartFrame=2\n";}
    const auto settings=cgl::LoadSettings(path.string());
    ASSERT_TRUE(settings);
    EXPECT_TRUE(settings->animation.Paused);EXPECT_EQ(settings->animation.StartFrame,2);
    const auto request=cgl::AnimationPreviewRequest(settings->animation.Version,settings->animation.SerialNum,
        settings->animation.Direction,settings->animation.Motion);
    EXPECT_EQ(uint32_t(request.version),3);EXPECT_EQ(request.serial,110300);EXPECT_EQ(request.direction,7);EXPECT_EQ(request.motion,1);
}
