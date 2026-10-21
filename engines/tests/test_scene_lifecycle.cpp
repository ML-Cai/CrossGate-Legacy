#include "assets/map_library.h"
#include "benchmark/crowd_benchmark.h"
#include "map/map_camera.h"
#include "map/map_render_data.h"
#include "map/map_hover.h"
#include "map/character_movement.h"
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
        world_.addSingleton<cgl::component::CursorPositionState>();
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
        maps_.clear();
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
    cgl::MapLibrary maps_;
    cgl::SceneManagerSystem manager_{[this](cgl::SceneTypes type) {
        return cgl::CreateScene(type, library_, &maps_);
    }};
    cgl::FrameRenderDrawSystem draw_;
    cgl::WindowInputSystem input_;
    std::string initialDiagnostic_;
};

TEST_F(DesktopSceneTest, HiddenWindowPollingClearsStaleCursorPresence) {
    auto* window = static_cast<GLFWwindow*>(world_.getSingleton<cgl::component::WindowHandle>()->nativeHandle);
    ASSERT_EQ(glfwGetWindowAttrib(window, GLFW_VISIBLE), GLFW_FALSE);
    auto* cursor = world_.getSingleton<cgl::component::CursorPositionState>();
    cursor->inside = true;
    cursor->windowWidth = cursor->windowHeight = 0;
    input_.update(&world_); // Initial publication without needing a cursor callback.
    EXPECT_FALSE(cursor->inside);
    int width = 0, height = 0;
    glfwGetWindowSize(window, &width, &height);
    EXPECT_EQ(cursor->windowWidth, width);
    EXPECT_EQ(cursor->windowHeight, height);
}

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
TEST_F(DesktopSceneTest, MapObjectsAndCharacterRestartWithoutEntityOrUploadLeaks) {
    world_.getSingleton<cgl::component::EngineState>()->settings->map.FilePath = "map/0/2100.dat";
    auto* scene = world_.getSingleton<cgl::component::SceneState>();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while ((!scene->scene || scene->scene->sceneType() != cgl::SceneTypes::MapScene) &&
           std::chrono::steady_clock::now() < deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state, cgl::StateTypes::ERROR) << scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene, nullptr);
    ASSERT_EQ(scene->scene->sceneType(), cgl::SceneTypes::MapScene);
    auto* camera = world_.getSingleton<cgl::component::MapCamera>();
    ASSERT_NE(camera, nullptr);
    EXPECT_DOUBLE_EQ(camera->center.x, 50);
    EXPECT_DOUBLE_EQ(camera->center.y, 50);
    auto* pass = world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* device = world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto& readers = *world_.getSingleton<cgl::component::AssetsReaderState>();
    const auto path = std::filesystem::path(world_.getSingleton<cgl::component::EngineState>()->settings->general.EngineRootPath) / "map/0/2100.dat";
    auto original = maps_.acquire(path, readers, device, pass);
    ASSERT_NE(original->renderer, nullptr);
    EXPECT_EQ(original->layout.width, 100);
    EXPECT_EQ(original->layout.height, 100);
    EXPECT_FALSE(original->images.empty());
    EXPECT_TRUE(original->images.contains(10058)); // Building image, not just ground.
    EXPECT_FALSE(original->images.contains(2));
    EXPECT_FALSE(original->images.contains(251));
    const cgl::Viewport viewport{0,0,800,600};
    auto* cursor = world_.getSingleton<cgl::component::CursorPositionState>();
    ASSERT_NE(world_.getSingleton<cgl::component::MapHover>(), nullptr);
    EXPECT_FALSE(world_.getSingleton<cgl::component::MapHover>()->cell);
    const auto hoverPoint = cgl::MapToScreen({50,49}, *camera, viewport);
    cursor->windowWidth = 400; cursor->windowHeight = 300;
    cursor->x = hoverPoint.x / 2; cursor->y = hoverPoint.y / 2;
    cursor->inside = true;
    manager_.update(&world_);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>()->cell, (cgl::MapCellCoord{50,49}));
    for (int i = 0; i < 3; ++i) draw_.update(&world_);
    cursor->inside = false;
    manager_.update(&world_);
    EXPECT_FALSE(world_.getSingleton<cgl::component::MapHover>()->cell);
    cursor->inside = true;
    cursor->x = 400; // Outside logical client bounds.
    manager_.update(&world_);
    EXPECT_FALSE(world_.getSingleton<cgl::component::MapHover>()->cell);
    cursor->x = hoverPoint.x / 2;
    std::weak_ptr<const cgl::MapHoverOutline> originalOutline = maps_.acquireHoverOutline(camera->scale, device, pass);
    std::weak_ptr<const cgl::AnimationClip> originalClip;
    bool firstCharacterVisit = true;
    auto verifyCharacter = [&] {
        unsigned count = 0;
        world_.forEach<cgl::component::MapPosition, cgl::component::Sprite, cgl::component::AnimationPlayer>(
            [&](auto, auto* position, auto* sprite, auto*) {
                ++count;
                ASSERT_NE(sprite->clip, nullptr);
                if (firstCharacterVisit) { originalClip = sprite->clip; firstCharacterVisit = false; }
                EXPECT_FALSE(originalClip.expired());
                EXPECT_EQ(originalClip.lock(), sprite->clip);
                EXPECT_DOUBLE_EQ(position->cell.x, 50);
                EXPECT_DOUBLE_EQ(position->cell.y, 50);
                EXPECT_LT(sprite->clip->offsetY, 0);
            });
        EXPECT_EQ(count, 1);
    };
    verifyCharacter();
    for (int restart = 0; restart < 3; ++restart) {
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX = 1;
        world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds = 0.5;
        manager_.update(&world_);
        EXPECT_GT(world_.getSingleton<cgl::component::MapCamera>()->center.x, 50);
        EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>()->cell,
            cgl::PickMapCell(hoverPoint, *world_.getSingleton<cgl::component::MapCamera>(), viewport, original->layout));
        for (int i = 0; i < 4; ++i) draw_.update(&world_);
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX = 0;
        world_.getSingleton<cgl::component::KeyboardInputState>()->restartSceneRequested = true;
        manager_.update(&world_);
        manager_.update(&world_);
        ASSERT_NE(scene->state, cgl::StateTypes::ERROR) << scene->lastError;
        ASSERT_NE(world_.getSingleton<cgl::component::MapCamera>(), nullptr);
        EXPECT_DOUBLE_EQ(world_.getSingleton<cgl::component::MapCamera>()->center.x, 50);
        EXPECT_EQ(maps_.acquire(path, readers, device, pass), original);
        verifyCharacter();
        EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>()->cell, (cgl::MapCellCoord{50,49}));
        EXPECT_EQ(maps_.acquireHoverOutline(1, device, pass), originalOutline.lock());
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX = 1;
        manager_.update(&world_);
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX = 0;
        world_.forEach<cgl::component::MapPosition, cgl::component::CharacterMovement,
            cgl::component::Sprite>([&](auto, auto* position, auto* movement, auto* sprite) {
            EXPECT_GT(position->cell.x, 50);
            EXPECT_GT(position->cell.y, 50);
            EXPECT_TRUE(movement->moving);
            EXPECT_EQ(movement->facing, 3);
            EXPECT_EQ(sprite->clip, movement->walk[3]);
            EXPECT_EQ(sprite->clip, library_.acquire({world_.getSingleton<cgl::component::EngineState>()->settings->animation.SerialNum, 3, 1}, readers, device, pass));
        });
        draw_.update(&world_); // Submit the walking renderer before switching clips.
        manager_.update(&world_);
        world_.forEach<cgl::component::CharacterMovement, cgl::component::Sprite>(
            [&](auto, auto* movement, auto* sprite) {
            EXPECT_FALSE(movement->moving);
            EXPECT_EQ(movement->facing, 3);
            EXPECT_EQ(sprite->clip, movement->idle[3]);
        });
        draw_.update(&world_);
        constexpr int directions[8][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
        for (const auto& direction : directions) {
            auto* currentCamera = world_.getSingleton<cgl::component::MapCamera>();
            const auto groundBefore = cgl::MapToScreen({40,40}, *currentCamera, viewport);
            auto* input = world_.getSingleton<cgl::component::KeyboardInputState>();
            input->moveX = float(direction[0]); input->moveY = float(direction[1]);
            world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds = .01;
            manager_.update(&world_);
            input->moveX = input->moveY = 0;
            world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds = .5;
            manager_.update(&world_);
            currentCamera = world_.getSingleton<cgl::component::MapCamera>();
            world_.forEach<cgl::component::MapPosition>([&](auto, auto* position) {
                const auto screen = cgl::MapToScreen(position->cell, *currentCamera, viewport);
                EXPECT_NEAR(screen.x, viewport.width * .5, 1e-9);
                EXPECT_NEAR(screen.y, viewport.height * .5, 1e-9);
            });
            const auto groundAfter = cgl::MapToScreen({40,40}, *currentCamera, viewport);
            const int mx = direction[0] - direction[1], my = direction[0] + direction[1];
            const auto delta = cgl::MapToWorld({double((mx>0)-(mx<0)),double((my>0)-(my<0))});
            EXPECT_NEAR(groundAfter.x - groundBefore.x, -delta.x, 1e-9);
            EXPECT_NEAR(groundAfter.y - groundBefore.y, -delta.y, 1e-9);
            EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>()->cell,
                cgl::PickMapCell(hoverPoint, *currentCamera, viewport, original->layout));
            draw_.update(&world_);
        }
        auto* input = world_.getSingleton<cgl::component::KeyboardInputState>();
        input->moveX = input->moveY = 0;
    }
    device->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapCamera>(), nullptr);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>(), nullptr);
    EXPECT_FALSE(originalOutline.expired());
    unsigned remaining = 0;
    world_.forEach<cgl::component::MapPosition>([&](auto, auto*) { ++remaining; });
    EXPECT_EQ(remaining, 0);
    std::weak_ptr<const cgl::MapResources> weak = original;
    original.reset();
    EXPECT_FALSE(weak.expired());
    maps_.clear();
    EXPECT_TRUE(weak.expired());
    EXPECT_TRUE(originalOutline.expired());
    EXPECT_FALSE(originalClip.expired());
    library_.clear();
    EXPECT_TRUE(originalClip.expired());
}

}

TEST_F(DesktopSceneTest, HouseFrontWallUsesNativeFootprintForActorOcclusion) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/2100.dat"; settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) &&
        std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr); ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto map=maps_.acquire(std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath,readers,device,pass);
    ASSERT_EQ(map->layout.cell(50,61)->objectId,11283);
    const auto& image=map->images.at(11283);
    ASSERT_EQ(image.tileX,4); ASSERT_EQ(image.tileY,6);
    const cgl::MapDrawItem house{{50,61},map->renderer.get(),image.source,0,image.offsetX,image.offsetY,1,image.tileX,image.tileY};
    for(const auto cell : {cgl::MapPoint{49,58},cgl::MapPoint{54,58}}) {
        world_.forEach<cgl::component::MapPosition,cgl::component::CharacterMovement,cgl::component::Sprite>(
            [&](auto,auto* position,auto* movement,auto* sprite) {
                position->cell=cell; movement->initialized=false;
                movement->stepping=false;movement->moving=false;
                const auto& clip=sprite->clip;
                cgl::MapDrawItem actor{cell,clip->renderer.get(),{0,0,clip->width,clip->height},0,
                    clip->offsetX,clip->offsetY,2,0,0};
                std::vector<cgl::MapDrawItem> pair{house,actor};
                EXPECT_EQ(cgl::SortMapDrawItems(pair),0);
                EXPECT_EQ(pair.back().stableId,cell.x<50?2:1);
            });
        world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds=.1;
        manager_.update(&world_);draw_.update(&world_);
    }
}

TEST_F(DesktopSceneTest, CrowdRestartAndShutdownReleaseAllEntitiesAndClips) {
    auto* engine=world_.getSingleton<cgl::component::EngineState>();
    engine->settings->map.FilePath="map/0/2100.dat";
    engine->settings->benchmark.ActorsPerCell=1;
    engine->settings->benchmark.UniqueCharacters=10;
    world_.addSingleton<cgl::component::CrowdBenchmark>();
    auto* benchmark=world_.getSingleton<cgl::component::CrowdBenchmark>();
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!benchmark->ready && std::chrono::steady_clock::now()<deadline) {
        if (world_.getSingleton<cgl::component::AssetsReaderState>()->state==cgl::StateTypes::RUNNING &&
            (!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene))
            scene->nextScene=cgl::SceneTypes::MapScene;
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(benchmark->ready);
    ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    const auto expected=benchmark->actors;
    ASSERT_GT(expected,0);EXPECT_EQ(expected,benchmark->visibleCells);
    std::weak_ptr<const cgl::AnimationClip> clip;
    for(int i=0;i<3;++i) {
        size_t actors=0;
        world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){++actors;clip=sprite->clip;});
        EXPECT_EQ(actors,expected);
        draw_.update(&world_);
        scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        EXPECT_EQ(benchmark->actors,expected);
    }
    EXPECT_THROW(library_.setTextureBudget(1),std::invalid_argument);
    EXPECT_FALSE(clip.expired());
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    size_t remaining=0;
    world_.forEach<cgl::component::CrowdActor>([&](auto,auto*){++remaining;});
    EXPECT_EQ(remaining,0);
    EXPECT_FALSE(clip.expired());library_.clear();EXPECT_TRUE(clip.expired());
}
