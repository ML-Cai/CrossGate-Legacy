#include <gtest/gtest.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "assets/assets_components.h"
#include "assets/animation_library.h"
#include "animation/animation_components.h"
#include "render/frame_render_systems.h"
#include "render/render_systems.h"
#include "scene/scene_systems.h"
#include "window/window_components.h"
#include "window/window_systems.h"
#include <chrono>
#include <algorithm>
#include <thread>
#include <vector>

namespace {
struct Events {
    std::vector<std::string> calls;
    bool requestOnEnter = false;
    bool failAnimation = false;
};
class TrackedScene final : public cgl::IScene {
public:
    TrackedScene(cgl::SceneTypes type, Events& events) : IScene(type), events_(events) {}
    ~TrackedScene() override { record("destroy"); }
    void onEnter(cgl::ECSCore* world) noexcept override {
        record("enter");
        auto* state = world->getSingleton<cgl::component::SceneState>();
        if (sceneType() == cgl::SceneTypes::InitScene && events_.requestOnEnter)
            state->nextScene = cgl::SceneTypes::AnimationScene;
        if (sceneType() == cgl::SceneTypes::AnimationScene && events_.failAnimation)
            state->state = cgl::StateTypes::ERROR;
    }
    void onExit(cgl::ECSCore*) noexcept override { record("exit"); }
    void update(cgl::ECSCore*) noexcept override {}
private:
    void record(const char* action) {
        events_.calls.push_back(std::string(action) + (sceneType() == cgl::SceneTypes::InitScene ? " init" : " animation"));
    }
    Events& events_;
};
class SceneManagerTest : public testing::Test {
protected:
    void SetUp() override {
        world_.addSingleton<cgl::component::SceneState>(cgl::StateTypes::RUNNING,
            cgl::SceneTypes::Unknown, nullptr, "");
        manager_.update(&world_);
    }
    void TearDown() override { cgl::SceneManagerDestroySystem{}.update(&world_); }
    auto* state() { return world_.getSingleton<cgl::component::SceneState>(); }
    cgl::ECSCore world_;
    Events events_;
    cgl::SceneManagerSystem manager_{[this](cgl::SceneTypes type) -> cgl::IScene::Ptr {
        if (type != cgl::SceneTypes::InitScene && type != cgl::SceneTypes::AnimationScene) return nullptr;
        return std::make_unique<TrackedScene>(type, events_);
    }};
};

TEST_F(SceneManagerTest, PreservesTransitionRequestedOnEnter) {
    events_.requestOnEnter = true;
    manager_.update(&world_);
    ASSERT_EQ(state()->nextScene, cgl::SceneTypes::AnimationScene);
    manager_.update(&world_);
    EXPECT_EQ(state()->scene->sceneType(), cgl::SceneTypes::AnimationScene);
    EXPECT_EQ(events_.calls, (std::vector<std::string>{"enter init", "exit init", "destroy init", "enter animation"}));
}
TEST_F(SceneManagerTest, RestartsSameSceneThroughExitAndEntry) {
    manager_.update(&world_);
    for (int i = 0; i < 3; ++i) {
        state()->nextScene = cgl::SceneTypes::InitScene;
        manager_.update(&world_);
    }
    EXPECT_EQ(std::count(events_.calls.begin(), events_.calls.end(), "enter init"), 4);
    EXPECT_EQ(std::count(events_.calls.begin(), events_.calls.end(), "exit init"), 3);
    EXPECT_EQ(std::count(events_.calls.begin(), events_.calls.end(), "destroy init"), 3);
}
TEST_F(SceneManagerTest, UnsupportedDestinationDoesNotExitCurrentScene) {
    manager_.update(&world_);
    auto* current = state()->scene.get();
    state()->nextScene = cgl::SceneTypes::MainScene;
    manager_.update(&world_);
    EXPECT_EQ(state()->state, cgl::StateTypes::ERROR);
    EXPECT_EQ(state()->scene.get(), current);
    EXPECT_EQ(events_.calls, (std::vector<std::string>{"enter init"}));
}
TEST_F(SceneManagerTest, FailedEntryStillGetsShutdownCleanup) {
    manager_.update(&world_);
    events_.failAnimation = true;
    state()->nextScene = cgl::SceneTypes::AnimationScene;
    manager_.update(&world_);
    ASSERT_EQ(state()->state, cgl::StateTypes::ERROR);
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(events_.calls.back(), "destroy animation");
    EXPECT_EQ(events_.calls[events_.calls.size() - 2], "exit animation");
    EXPECT_EQ(state()->scene, nullptr);
}

class DesktopSceneTest : public testing::Test {
protected:
    void SetUp() override {
        auto settings = cgl::LoadSettings("settings.ini");
        ASSERT_NE(settings, nullptr);
        world_.addSingleton<cgl::component::EngineState>(cgl::StateTypes::RUNNING, "", std::move(settings));
        world_.addSingleton<cgl::component::WindowState>(cgl::StateTypes::UNKNOWN, "");
        world_.addSingleton<cgl::component::WindowCreateInfo>(800, 600, "scene test");
        world_.addSingleton<cgl::component::KeyboardInputState>();
        world_.addSingleton<cgl::component::FrameTime>();
        world_.addSingleton<cgl::component::RenderDeviceState>(cgl::StateTypes::UNKNOWN, "");
        world_.addSingleton<cgl::component::FrameRenderState>(cgl::StateTypes::INITIALIZING, "");
        world_.addSingleton<cgl::component::AssetsReaderState>(cgl::StateTypes::UNKNOWN, "");
        world_.addSingleton<cgl::component::SceneState>(cgl::StateTypes::RUNNING, cgl::SceneTypes::Unknown, nullptr, "");
        ASSERT_EQ(glfwInit(), GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        cgl::WindowInitSystem{}.update(&world_);
        ASSERT_NE(world_.getSingleton<cgl::component::WindowHandle>(), nullptr);
        world_.getSingleton<cgl::component::WindowState>()->state = cgl::StateTypes::RUNNING;
        cgl::RenderDeviceInitSystem{}.update(&world_);
        auto& render = *world_.getSingleton<cgl::component::RenderDeviceState>();
        ASSERT_NE(render.pDevice, nullptr);
        render.state = cgl::StateTypes::RUNNING;
        render.pDevice->getLastError(&initialDiagnostic_);
        cgl::FrameRenderInitSystem{}.update(&world_);
        auto* frame = world_.getSingleton<cgl::component::FrameRenderState>();
        ASSERT_NE(frame->state, cgl::StateTypes::ERROR) << frame->lastError;
        frame->state = cgl::StateTypes::RUNNING;
        manager_.update(&world_);
    }
    void TearDown() override {
        auto* render = world_.getSingleton<cgl::component::RenderDeviceState>();
        if (render && render->pDevice) render->pDevice->waitIdle();
        cgl::SceneManagerDestroySystem{}.update(&world_);
        // Keep teardown safe even when an entity-cleanup assertion fails.
        std::vector<cgl::Entity> leftovers;
        world_.forEach<cgl::component::Sprite>([&](auto entity, auto*) { leftovers.push_back(entity); });
        for (auto entity : leftovers) world_.destroyEntity(entity);
        library_.clear();
        if (world_.getSingleton<cgl::component::FrameRenderState>()) cgl::FrameRenderDestroySystem{}.update(&world_);
        if (render && render->pDevice) {
            std::string diagnostic;
            render->pDevice->getLastError(&diagnostic);
            EXPECT_EQ(diagnostic, initialDiagnostic_) << "New Vulkan validation error";
        }
        if (render) cgl::RenderDeviceDestroySystem{}.update(&world_);
        if (auto* window = world_.getSingleton<cgl::component::WindowHandle>())
            glfwDestroyWindow(static_cast<GLFWwindow*>(window->nativeHandle));
        glfwTerminate();
    }
    cgl::ECSCore world_;
    cgl::AnimationLibrary library_;
    cgl::SceneManagerSystem manager_{library_};
    cgl::FrameRenderDrawSystem draw_;
    std::string initialDiagnostic_;
};

TEST_F(DesktopSceneTest, RepeatedRestartKeepsFrameResourcesAndReaders) {
    auto* scene = world_.getSingleton<cgl::component::SceneState>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((!scene->scene || scene->scene->sceneType() != cgl::SceneTypes::AnimationScene) &&
           std::chrono::steady_clock::now() < deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state, cgl::StateTypes::ERROR) << scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene, nullptr);
    ASSERT_EQ(scene->scene->sceneType(), cgl::SceneTypes::AnimationScene);
    auto* frame = world_.getSingleton<cgl::component::PrimarySceneRenderFrame>();
    auto* pass = frame->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* readers = world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* reader = readers->pAnimeDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get();
    std::weak_ptr<const cgl::AnimationClip> original;
    bool firstVisit = true;
    auto verifySprites = [&] {
        std::vector<cgl::Entity> entities;
        world_.forEach<cgl::component::Sprite>([&](auto entity, auto*) { entities.push_back(entity); });
        ASSERT_EQ(entities.size(), 2);
        auto& first = *world_.getComponent<cgl::component::Sprite>(entities[0]);
        auto& second = *world_.getComponent<cgl::component::Sprite>(entities[1]);
        EXPECT_EQ(first.clip, second.clip);
        if (firstVisit) { original = first.clip; firstVisit = false; }
        EXPECT_FALSE(original.expired());
        EXPECT_EQ(original.lock(), first.clip);
        EXPECT_NE(world_.getComponent<cgl::component::AnimationPlayer>(entities[0])->frame,
                  world_.getComponent<cgl::component::AnimationPlayer>(entities[1])->frame);
    };
    verifySprites();
    for (int restart = 0; restart < 3; ++restart) {
        for (int i = 0; i < 4; ++i) draw_.update(&world_);
        auto* oldScene = scene->scene.get();
        world_.getSingleton<cgl::component::KeyboardInputState>()->restartSceneRequested = true;
        manager_.update(&world_); // The animation scene requests its own replacement.
        manager_.update(&world_); // The manager exits and recreates it.
        ASSERT_NE(scene->state, cgl::StateTypes::ERROR) << scene->lastError;
        EXPECT_NE(scene->scene.get(), oldScene);
        EXPECT_EQ(world_.getSingleton<cgl::component::PrimarySceneRenderFrame>(), frame);
        EXPECT_EQ(frame->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get(), pass);
        EXPECT_EQ(readers->pAnimeDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get(), reader);
        EXPECT_NE(world_.getSingleton<cgl::component::FrameRenderState>()->state, cgl::StateTypes::ERROR);
        verifySprites(); // Detect entity accumulation or clip re-upload on restart.
    }
    draw_.update(&world_);
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    unsigned remaining = 0;
    world_.forEach<cgl::component::Sprite>([&](auto, auto*) { ++remaining; });
    EXPECT_EQ(remaining, 0);
    EXPECT_FALSE(original.expired()); // Cache survives scene exit.
    library_.clear();
    EXPECT_TRUE(original.expired()); // Final cache cleanup releases the GPU asset.
}
}
