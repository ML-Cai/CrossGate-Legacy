#include "assets/map_library.h"
#include "assets/character_catalog.h"
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
#include "map/remote_characters.h"
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
    bool enterReadyScene(cgl::SceneTypes type) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        auto* scene=world_.getSingleton<cgl::component::SceneState>();
        while(std::chrono::steady_clock::now()<deadline) {
            manager_.update(&world_);
            if(scene->state==cgl::StateTypes::ERROR) return false;
            auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();
            if(scene->scene && scene->scene->sceneType()==type && progress && progress->ready) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }
    cgl::ECSCore world_;
    cgl::AnimationLibrary library_;
    cgl::MapLibrary maps_;
    std::function<void()> maintenance_;
    cgl::SceneManagerSystem manager_{[this](cgl::SceneTypes type) {
        return cgl::CreateScene(type, library_, &maps_);
    },[this]{if(maintenance_) maintenance_();}};
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
    ASSERT_FALSE(original->renderers.empty());
    ASSERT_NE(original->renderers.front(), nullptr);
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
    // Drawing may use the new pointer snapshot for presentation, but the world
    // selection remains the result of its last update until update runs again.
    draw_.update(&world_);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>()->cell, (cgl::MapCellCoord{50,49}));
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
    const cgl::MapDrawItem house{{50,61},map->renderers.at(image.atlasIndex).get(),image.source,0,image.offsetX,image.offsetY,1,image.tileX,image.tileY};
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

TEST_F(DesktopSceneTest, LargeMap100UsesMultipleAtlasesAndReusesThemOnRestart) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";
    settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) &&
          std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);
    ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto path=std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath;
    auto map=maps_.acquire(path,readers,device,pass);
    ASSERT_EQ(map->layout.width,840); ASSERT_EQ(map->layout.height,610);
    EXPECT_EQ(map->rgbaBytes,40330752);
    EXPECT_EQ(maps_.usage().maps,1);EXPECT_EQ(maps_.usage().rgbaBytes,map->rgbaBytes);
    EXPECT_EQ(maps_.usage().pages,map->renderers.size());
    ASSERT_GT(map->renderers.size(),1); ASSERT_LE(map->renderers.size(),8);
    std::vector<bool> used(map->renderers.size(),false);
    for(const auto& [id,image] : map->images) {
        ASSERT_LT(image.atlasIndex,map->renderers.size());
        ASSERT_NE(map->renderers[image.atlasIndex],nullptr);
        EXPECT_GT(image.source.width,0); EXPECT_GT(image.source.height,0);
    }
    // Visit anchors from every page, so culling also submits later-page renderers.
    auto* camera=world_.getSingleton<cgl::component::MapCamera>();
    for(uint32_t y=0;y<map->layout.height;++y) for(uint32_t x=0;x<map->layout.width;++x) {
        const auto& cell=*map->layout.cell(x,y);
        const uint16_t ids[]{cell.groundId,cell.objectId};
        for(int layer=0;layer<2;++layer) {
            const auto id=ids[layer];
            if(!cgl::IsClassicMapImage(id,layer==1)) continue;
            const auto found=map->images.find(id);
            if(found==map->images.end() || used[found->second.atlasIndex]) continue;
            used[found->second.atlasIndex]=true;
            camera->center={double(x),double(y)};
            draw_.update(&world_);
        }
    }
    EXPECT_TRUE(std::all_of(used.begin(),used.end(),[](bool v){return v;}));
    std::vector<cgl::graphics::ISpriteRenderer*> pages;
    for(const auto& renderer:map->renderers) pages.push_back(renderer.get());
    for(int restart=0;restart<2;++restart) {
        world_.getSingleton<cgl::component::KeyboardInputState>()->restartSceneRequested=true;
        manager_.update(&world_);manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        EXPECT_EQ(maps_.acquire(path,readers,device,pass),map);
        for(size_t i=0;i<pages.size();++i) EXPECT_EQ(map->renderers[i].get(),pages[i]);
        world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds=.1;
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX=1;
        manager_.update(&world_);
        world_.getSingleton<cgl::component::KeyboardInputState>()->moveX=0;
        manager_.update(&world_);
        draw_.update(&world_);
    }
    device->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    std::weak_ptr<const cgl::MapResources> weak=map;map.reset();
    EXPECT_FALSE(weak.expired());maps_.clear();EXPECT_TRUE(weak.expired());
    EXPECT_EQ(maps_.usage().maps,0);EXPECT_EQ(maps_.usage().rgbaBytes,0);EXPECT_EQ(maps_.usage().outlineBytes,0);
}

TEST_F(DesktopSceneTest, ColdCrowdPublishesClipsAndCanExitWithPendingResources) {
    auto* engine=world_.getSingleton<cgl::component::EngineState>();
    engine->settings->map.FilePath="map/0/2100.dat";
    auto& b=engine->settings->benchmark;
    b.ActorsPerCell=1;b.UniqueCharacters=4;b.LoadDuringPlayback=true;b.ProgressiveUploads=true;b.BackgroundDecode=true;b.WarmupFrames=0;
    world_.addSingleton<cgl::component::CrowdBenchmark>();
    auto* benchmark=world_.getSingleton<cgl::component::CrowdBenchmark>();
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!benchmark->ready && std::chrono::steady_clock::now()<deadline) {
        if(world_.getSingleton<cgl::component::AssetsReaderState>()->state==cgl::StateTypes::RUNNING &&
           (!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene)) scene->nextScene=cgl::SceneTypes::MapScene;
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(benchmark->ready);
    while(benchmark->unique<4 && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);draw_.update(&world_);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(benchmark->unique,4);
    EXPECT_EQ(benchmark->coldLoadMs.size(),3);
    world_.forEach<cgl::component::CrowdActor,cgl::component::Sprite>([&](auto,auto* actor,auto* sprite){
        EXPECT_EQ(actor->idle,sprite->clip); EXPECT_NE(sprite->clip,nullptr);
    });
    b.UniqueCharacters=20; // Force uncached work beyond the warmed four-clip set.
    scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    ASSERT_LT(benchmark->unique,20);
    for(int i=0;i<20 && !library_.pendingBytes();++i) manager_.update(&world_);
    EXPECT_GT(library_.pendingBytes(),0); // CPU running/ready or GPU upload, never published yet.
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    size_t remaining=0;world_.forEach<cgl::component::CrowdActor>([&](auto,auto*){++remaining;});
    EXPECT_EQ(remaining,0);EXPECT_EQ(library_.pendingBytes(),0);
}

TEST_F(DesktopSceneTest, ProgressiveLibraryDeduplicatesBoundsAndCancelsPendingUpload) {
    cgl::AnimationLibrary progressive;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(world_.getSingleton<cgl::component::AssetsReaderState>()->state!=cgl::StateTypes::RUNNING &&
          std::chrono::steady_clock::now()<deadline) {manager_.update(&world_);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    ASSERT_EQ(world_.getSingleton<cgl::component::AssetsReaderState>()->state,cgl::StateTypes::RUNNING);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest request{100000,0,0};
    EXPECT_EQ(progressive.acquireProgressive(request,readers,device,pass),nullptr);
    cgl::component::AssetsReaderState otherReaders{};
    EXPECT_THROW(progressive.acquireProgressive(request,otherReaders,device,pass),std::invalid_argument);
    ASSERT_GT(progressive.pendingBytes(),0);EXPECT_LE(progressive.pendingBytes(),32*1024*1024);
    EXPECT_EQ(progressive.usage().clips,0);EXPECT_EQ(progressive.usage().pendingBytes,progressive.pendingBytes());
    EXPECT_THROW(progressive.setTextureBudget(1),std::invalid_argument);
    std::shared_ptr<const cgl::AnimationClip> clip;
    while(!clip && std::chrono::steady_clock::now()<deadline) {
        clip=progressive.acquireProgressive(request,readers,device,pass);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(clip,nullptr);EXPECT_EQ(progressive.pendingBytes(),0);
    EXPECT_EQ(progressive.usage().clips,1);
    EXPECT_EQ(progressive.usage().rgbaBytes,uint64_t(clip->width)*clip->height*clip->frameCount*4);
    EXPECT_EQ(progressive.acquireProgressive(request,readers,device,pass),clip);
    const cgl::AnimationRequest next{100000,0,1};
    EXPECT_EQ(progressive.acquireProgressive(next,readers,device,pass),nullptr);
    ASSERT_GT(progressive.pendingBytes(),0);
    progressive.cancelPending();EXPECT_EQ(progressive.pendingBytes(),0);
    EXPECT_EQ(progressive.acquireProgressive(next,readers,device,pass),nullptr);
    progressive.clear();EXPECT_EQ(progressive.pendingBytes(),0);
    EXPECT_EQ(progressive.usage().clips,0);EXPECT_EQ(progressive.usage().rgbaBytes,0);
    EXPECT_EQ(progressive.acquireProgressive(next,readers,device,pass),nullptr);
    progressive.cancelPending();
}

TEST_F(DesktopSceneTest, ServerSnapshotsKeepMovementAndReleaseRemoteEntities) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    settings.serverdemo.Enabled=true;
    library_.setTextureBudget(512ull*1024*1024);
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto count=[&]{size_t n=0;world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++n;});return n;};
    EXPECT_EQ(count(),288);
    auto* input=world_.getSingleton<cgl::component::KeyboardInputState>();
    auto* time=world_.getSingleton<cgl::component::FrameTime>();
    const auto start=world_.getSingleton<cgl::component::MapCamera>()->center;
    input->moveY=-1;time->deltaSeconds=.05;
    for(int i=0;i<60;++i) {manager_.update(&world_);draw_.update(&world_);}
    input->moveY=0;
    const auto moved=world_.getSingleton<cgl::component::MapCamera>()->center;
    EXPECT_GT(std::hypot(moved.x-start.x,moved.y-start.y),1);
    world_.forEach<cgl::component::CharacterMovement,cgl::component::MapPosition>([&](auto,auto*,auto* p){
        EXPECT_DOUBLE_EQ(p->cell.x,moved.x);EXPECT_DOUBLE_EQ(p->cell.y,moved.y);
    });
    for(int i=0;i<3;++i) {input->newBatchRequested=true;manager_.update(&world_);EXPECT_EQ(count(),288);draw_.update(&world_);}
    for(int i=0;i<2;++i) {
        input->restartSceneRequested=true;manager_.update(&world_);manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;EXPECT_EQ(count(),288);
    }
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    std::vector<std::weak_ptr<const cgl::AnimationClip>> clips;
    world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){clips.push_back(sprite->clip);});
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(count(),0);EXPECT_EQ(library_.pendingBytes(),0);
    library_.clear();
    for(const auto& clip:clips) EXPECT_TRUE(clip.expired());
}

TEST(RemoteCharacters, RejectsMalformedSnapshotBeforeMutatingWorld) {
    cgl::ECSCore world;
    cgl::RemoteCharacters remote({cgl::AnimationRequest{100000,0,0}});
    remote.apply(world,{1,{{1,{2,3},0},{2,{3,4},0}}},nullptr);
    auto count=[&]{size_t n=0;world.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++n;});return n;};
    EXPECT_EQ(count(),2);
    EXPECT_THROW(remote.apply(world,{2,{{3,{2,3},0},{3,{3,4},0}}},nullptr),std::invalid_argument);
    EXPECT_THROW(remote.apply(world,{2,{{3,{2,3},1}}},nullptr),std::invalid_argument);
    EXPECT_EQ(count(),2);
    remote.apply(world,{1,{}},nullptr);EXPECT_EQ(count(),2); // Stale packets ignored.
    remote.apply(world,{2,{{2,{5,6},0}}},nullptr);EXPECT_EQ(count(),1);
    world.forEach<cgl::component::RemoteCharacter,cgl::component::MapPosition>([&](auto,auto* actor,auto* p){
        EXPECT_EQ(actor->serverId,2);EXPECT_DOUBLE_EQ(p->cell.x,5);EXPECT_DOUBLE_EQ(p->cell.y,6);
    });
    remote.clear(world);EXPECT_EQ(count(),0);
}

TEST_F(DesktopSceneTest, SwitchBetweenServerDemoAndCrowdReleasesPreviousModeEntities) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    settings.serverdemo.Enabled=true;library_.setTextureBudget(512ull*1024*1024);
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto remoteCount=[&]{size_t n=0;world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++n;});return n;};
    auto crowdCount=[&]{size_t n=0;world_.forEach<cgl::component::CrowdActor>([&](auto,auto*){++n;});return n;};
    EXPECT_EQ(remoteCount(),288);EXPECT_EQ(crowdCount(),0);
    // Start a cold appearance, then leave while work may still be pending.
    manager_.update(&world_);
    settings.serverdemo.Enabled=false;settings.benchmark.ActorCount=16;settings.benchmark.UniqueCharacters=2;
    world_.addSingleton<cgl::component::CrowdBenchmark>();
    scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    EXPECT_EQ(remoteCount(),0);EXPECT_EQ(crowdCount(),16);EXPECT_EQ(library_.pendingBytes(),0);
    draw_.update(&world_);
    world_.removeSingleton<cgl::component::CrowdBenchmark>();
    settings.serverdemo.Enabled=true;settings.benchmark.ActorCount=0;
    scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    EXPECT_EQ(remoteCount(),288);EXPECT_EQ(crowdCount(),0);
    draw_.update(&world_);
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    std::vector<std::weak_ptr<const cgl::AnimationClip>> clips;
    world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){clips.push_back(sprite->clip);});
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(remoteCount(),0);EXPECT_EQ(crowdCount(),0);EXPECT_EQ(library_.pendingBytes(),0);
    library_.clear();for(const auto& clip:clips) EXPECT_TRUE(clip.expired());
}

TEST(SceneMaintenance, RunsAfterOldDestructionAndBeforeNewEntry) {
    cgl::ECSCore world;Events events;
    world.addSingleton<cgl::component::SceneState>(cgl::StateTypes::RUNNING,cgl::SceneTypes::Unknown,nullptr,"");
    auto* state=world.getSingleton<cgl::component::SceneState>();
    cgl::SceneManagerSystem manager{[&](auto type){return std::make_unique<TrackedScene>(type,events);},[&]{
        EXPECT_EQ(state->scene,nullptr);EXPECT_EQ(events.calls.back(),"destroy init");events.calls.push_back("maintenance");
    }};
    manager.update(&world);manager.update(&world);
    state->nextScene=cgl::SceneTypes::AnimationScene;manager.update(&world);
    EXPECT_EQ(events.calls,(std::vector<std::string>{"enter init","exit init","destroy init","maintenance","enter animation"}));
    cgl::SceneManagerDestroySystem{}.update(&world);
}
TEST(SceneMaintenance, FailureStopsNewEntryWithoutDoubleExit) {
    cgl::ECSCore world;Events events;
    world.addSingleton<cgl::component::SceneState>(cgl::StateTypes::RUNNING,cgl::SceneTypes::Unknown,nullptr,"");
    cgl::SceneManagerSystem manager{[&](auto type){return std::make_unique<TrackedScene>(type,events);},[]{throw std::runtime_error("maintenance failure");}};
    manager.update(&world);manager.update(&world);
    auto* state=world.getSingleton<cgl::component::SceneState>();state->nextScene=cgl::SceneTypes::AnimationScene;
    manager.update(&world);
    EXPECT_EQ(state->state,cgl::StateTypes::ERROR);EXPECT_EQ(state->scene,nullptr);
    EXPECT_EQ(std::count(events.calls.begin(),events.calls.end(),"enter animation"),0);
    cgl::SceneManagerDestroySystem{}.update(&world);
    EXPECT_EQ(std::count(events.calls.begin(),events.calls.end(),"exit init"),1);
}
TEST_F(DesktopSceneTest, IdleCacheTrimWaitsForDrawsPreservesPinsAndReloads) {
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::AnimationScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::AnimationScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    cgl::AnimationLibrary cache;
    auto pin=cache.acquire({100000,0,0},readers,device,pass);
    for(int round=0;round<3;++round) {
        auto clip=cache.acquire({100000,1,0},readers,device,pass);
        std::weak_ptr<const cgl::AnimationClip> weak=clip;
        auto entity=world_.createEntity();
        world_.addComponent<cgl::component::Transform2D>(entity);
        world_.addComponent<cgl::component::Sprite>(entity,clip);
        world_.addComponent<cgl::component::AnimationPlayer>(entity);
        draw_.update(&world_);world_.destroyEntity(entity);clip.reset();
        EXPECT_GT(cache.trimUnused(0),0);EXPECT_TRUE(weak.expired());EXPECT_EQ(cache.usage().clips,1);
    }
    EXPECT_EQ(cache.acquireProgressive({100000,2,0},readers,device,pass),nullptr);
    EXPECT_GT(cache.pendingBytes(),0);EXPECT_EQ(cache.trimUnused(0),0);EXPECT_GT(cache.pendingBytes(),0);
    cache.cancelPending();std::weak_ptr<const cgl::AnimationClip> pinned=pin;pin.reset();
    EXPECT_GT(cache.trimUnused(0),0);EXPECT_TRUE(pinned.expired());EXPECT_EQ(cache.usage().rgbaBytes,0);
    EXPECT_EQ(cache.usage().evictions,4);cache.clear();
    cgl::MapLibrary maps;
    const auto root=world_.getSingleton<cgl::component::EngineState>()->settings->general.EngineRootPath;
    auto map=maps.acquire(std::filesystem::path(root)/"map/0/100.dat",readers,device,pass);
    std::weak_ptr<const cgl::MapResources> idle=maps.acquire(std::filesystem::path(root)/"map/0/2100.dat",readers,device,pass);
    auto outline=maps.acquireHoverOutline(1,device,pass);
    std::weak_ptr<const cgl::MapHoverOutline> idleOutline=maps.acquireHoverOutline(2,device,pass);
    EXPECT_GT(maps.trimUnused(0,0),0);EXPECT_TRUE(idle.expired());EXPECT_TRUE(idleOutline.expired());
    EXPECT_EQ(maps.usage().maps,1);EXPECT_EQ(maps.usage().outlines,1);EXPECT_EQ(map->layout.width,840);
    map.reset();outline.reset();EXPECT_GT(maps.trimUnused(0,0),0);
    EXPECT_EQ(maps.usage().rgbaBytes,0);EXPECT_EQ(maps.usage().outlineBytes,0);
}

TEST_F(DesktopSceneTest, TransitionMaintenanceEvictsAfterExitAndReloadsBeforeDraw) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    std::weak_ptr<const cgl::AnimationClip> clip;
    std::weak_ptr<const cgl::MapResources> map;
    int trims=0;
    maintenance_=[&]{
        EXPECT_GT(library_.trimUnused(0),0);EXPECT_GT(maps_.trimUnused(0,0),0);
        EXPECT_TRUE(clip.expired());EXPECT_TRUE(map.expired());++trims;
    };
    for(int round=0;round<2;++round) {
        world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){clip=sprite->clip;});
        map=maps_.acquire(std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath,readers,device,pass);
        draw_.update(&world_);scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        EXPECT_EQ(library_.usage().clips,16);EXPECT_EQ(maps_.usage().maps,1);
        draw_.update(&world_);
    }
    EXPECT_EQ(trims,2);EXPECT_GE(library_.usage().evictions,32);
    maintenance_={};
}

TEST_F(DesktopSceneTest, DeferredRetirementFollowsActualFrameFencesAndRetainsBudget) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    // Keep one unrelated owner pinned: it must never enter retirement.
    auto pin=library_.acquire({100000,7,0},readers,device,pass);
    for(int round=0;round<3;++round) {
        if(round) {scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);}
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::vector<std::weak_ptr<const cgl::AnimationClip>> clips;
        world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){clips.push_back(sprite->clip);});
        std::weak_ptr<const cgl::MapResources> map=maps_.acquire(
            std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath,readers,device,pass);
        std::weak_ptr<const cgl::MapHoverOutline> outline=maps_.acquireHoverOutline(1,device,pass);
        draw_.update(&world_);draw_.update(&world_);
        ASSERT_GT(sync->progress.submitted,sync->progress.completed);
        cgl::SceneManagerDestroySystem{}.update(&world_);
        const auto submitted=sync->progress.submitted;
        const auto oldBytes=library_.usage().rgbaBytes;
        EXPECT_GT(library_.retireUnused(0,submitted),0);
        EXPECT_GT(maps_.retireUnused(0,submitted,0),0);
        EXPECT_EQ(library_.usage().clips,1);EXPECT_GT(library_.usage().retiredClips,0);
        EXPECT_EQ(library_.usage().rgbaBytes+library_.usage().retiredBytes,oldBytes);
        // Retired allocations cannot be silently removed from the hard budget.
        EXPECT_THROW(library_.setTextureBudget(library_.usage().rgbaBytes),std::invalid_argument);
        EXPECT_EQ(library_.collectRetired(sync->progress.completed),0);
        EXPECT_EQ(maps_.collectRetired(sync->progress.completed),0);
        EXPECT_FALSE(map.expired());EXPECT_FALSE(outline.expired());
        for(const auto& clip:clips) EXPECT_FALSE(clip.expired());
        // Submit blank scenes through the normal draw loop; only its successful
        // fence waits may advance completion. No device waitIdle in this path.
        for(size_t i=0;i<sync->objs.size()+1;++i) draw_.update(&world_);
        ASSERT_GE(sync->progress.completed,submitted);
        EXPECT_GT(library_.collectRetired(sync->progress.completed),0);
        EXPECT_GT(maps_.collectRetired(sync->progress.completed),0);
        EXPECT_TRUE(map.expired());EXPECT_TRUE(outline.expired());
        for(const auto& clip:clips) EXPECT_TRUE(clip.expired());
        EXPECT_EQ(library_.usage().retiredBytes,0);EXPECT_EQ(library_.usage().retiredClips,0);
        EXPECT_EQ(maps_.usage().retiredMaps,0);EXPECT_EQ(maps_.usage().retiredOutlines,0);
        EXPECT_NE(library_.findReady({100000,7,0}),nullptr);
        scene->state=cgl::StateTypes::RUNNING;
    }
}

TEST_F(DesktopSceneTest, BudgetPressureWaitsForPinsAndFenceCompletionThenRetries) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::AnimationScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::AnimationScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    const auto baseline=library_.usage().rgbaBytes;
    auto pin=library_.acquire({100000,0,0},readers,device,pass);
    const auto aBytes=library_.usage().rgbaBytes-baseline;
    cgl::AnimationBuilder plan;
    const cgl::AnimationRequest request{100000,1,0};
    const auto bBytes=plan.build(request,readers).rgba.size();
    library_.setTextureBudget(baseline+std::max<uint64_t>(aBytes,bBytes)+1);
    const auto entity=world_.createEntity();
    world_.addComponent<cgl::component::Transform2D>(entity);
    world_.addComponent<cgl::component::Sprite>(entity,pin);
    world_.addComponent<cgl::component::AnimationPlayer>(entity);
    draw_.update(&world_);draw_.update(&world_);
    while(!library_.waitingForBudget() && std::chrono::steady_clock::now()<deadline) {
        EXPECT_EQ(library_.acquireProgressive(request,readers,device,pass),nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(library_.waitingForBudget());EXPECT_EQ(library_.pendingBytes(),0);
    const auto reads=readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic]->cacheStatistics().fileReadCalls;
    for(int i=0;i<32;++i) {
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
        EXPECT_EQ(library_.acquireProgressive(request,readers,device,pass),nullptr);
    }
    EXPECT_EQ(library_.usage().evictions,0);EXPECT_EQ(library_.usage().retiredClips,0);
    EXPECT_EQ(readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic]->cacheStatistics().fileReadCalls,reads);
    // A cancelled/replaced wait must not reset the scan cooldown under churn.
    library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    library_.cancelPending();
    EXPECT_EQ(library_.acquireProgressive(request,readers,device,pass,false),nullptr);
    ASSERT_TRUE(library_.waitingForBudget());
    std::weak_ptr<const cgl::AnimationClip> weak=pin;
    world_.destroyEntity(entity);pin.reset();
    library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    EXPECT_EQ(library_.usage().retiredClips,0);
    library_.maintainStreaming(sync->progress.submitted,sync->progress.completed,false);
    EXPECT_EQ(library_.usage().retiredClips,0);
    for(int i=0;i<17 && !library_.usage().retiredClips;++i)
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    ASSERT_EQ(library_.usage().retiredClips,1);EXPECT_FALSE(weak.expired());
    EXPECT_EQ(library_.acquireProgressive(request,readers,device,pass),nullptr);
    EXPECT_EQ(library_.pendingBytes(),0); // Retirement has not freed budget yet.
    for(size_t i=0;i<sync->objs.size()+1;++i) draw_.update(&world_);
    library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    EXPECT_TRUE(weak.expired());EXPECT_EQ(library_.usage().retiredBytes,0);
    std::shared_ptr<const cgl::AnimationClip> ready;
    while(!ready && std::chrono::steady_clock::now()<deadline) {
        ready=library_.acquireProgressive(request,readers,device,pass);
        const auto usage=library_.usage();
        EXPECT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,usage.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(ready,nullptr);EXPECT_FALSE(library_.waitingForBudget());EXPECT_GT(library_.usage().budgetDeferrals,0);
}
TEST_F(DesktopSceneTest, RemoteBudgetWaitCanChangeCandidateAndRetryWithoutPermanentFailure) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::AnimationScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::AnimationScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto baseline=library_.usage().rgbaBytes;
    library_.setTextureBudget(baseline);
    const cgl::AnimationRequest request{100000,0,0};
    EXPECT_EQ(library_.acquireProgressive(request,readers,device,pass,false),nullptr);
    ASSERT_TRUE(library_.waitingForBudget());EXPECT_EQ(library_.pendingBytes(),0);
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    cgl::RemoteCharacters remote({request});
    cgl::component::MapCamera camera;camera.center={10,10};const cgl::Viewport viewport{0,0,800,600};
    remote.apply(world_,{1,{{1,{10,10},0}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(remote.visible,1);EXPECT_EQ(remote.readyVisible,0);
    remote.apply(world_,{2,{}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_FALSE(library_.waitingForBudget());EXPECT_EQ(library_.pendingBytes(),0);
    // This exact appearance must be retried when it returns with available budget.
    library_.setTextureBudget(baseline+32ull*1024*1024);
    remote.apply(world_,{3,{{2,{10,10},0}}},placeholder);
    while(!remote.readyVisible && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(remote.readyVisible,1);EXPECT_NE(library_.findReady(request),nullptr);
    remote.clear(world_);
    // A clip too large for the TOTAL configured budget remains an actual error.
    cgl::AnimationLibrary tooSmall;tooSmall.setTextureBudget(1);
    EXPECT_THROW(tooSmall.acquireProgressive(request,readers,device,pass,false),std::runtime_error);
    EXPECT_FALSE(tooSmall.waitingForBudget());EXPECT_EQ(tooSmall.pendingBytes(),0);
    EXPECT_THROW(library_.acquireProgressive({0,0,0},readers,device,pass,false),std::runtime_error);
    EXPECT_FALSE(library_.waitingForBudget());
}

TEST_F(DesktopSceneTest, RepeatedDistinctAppearanceBatchesRecoverUnderSmallWorkingSetBudget) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(40);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::AnimationScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::AnimationScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    auto catalog=cgl::ReadCharacterCatalog(readers,8);ASSERT_EQ(catalog.size(),8);
    cgl::AnimationBuilder plan;uint64_t largest=0;
    for(const auto& request:catalog) largest=std::max<uint64_t>(largest,plan.build(request,readers).rgba.size());
    const auto baseline=library_.usage().rgbaBytes;
    library_.setTextureBudget(baseline+largest);
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    cgl::RemoteCharacters remote(catalog);cgl::component::MapCamera camera;camera.center={10,10};
    const cgl::Viewport viewport{0,0,800,600};
    for(uint64_t batch=1;batch<=16;++batch) {
        remote.apply(world_,{batch,{{batch,{10,10},size_t(batch-1)%catalog.size()}}},placeholder);
        world_.forEach<cgl::component::RemoteCharacter>([&](auto entity,auto*) {
            world_.addComponent<cgl::component::Transform2D>(entity);
        });
        remote.readyVisible=0;
        while(!remote.readyVisible && std::chrono::steady_clock::now()<deadline) {
            library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
            remote.update(world_,library_,readers,device,pass,camera,viewport);draw_.update(&world_);
            const auto usage=library_.usage();
            ASSERT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,usage.budgetBytes);
            ASSERT_NE(world_.getSingleton<cgl::component::FrameRenderState>()->state,cgl::StateTypes::ERROR);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_EQ(remote.readyVisible,1)<<"Appearance batch "<<batch;
    }
    EXPECT_GT(library_.usage().budgetDeferrals,0);EXPECT_GT(library_.usage().evictions,0);
    remote.clear(world_);library_.cancelPending();
    for(size_t i=0;i<sync->objs.size()+1;++i) draw_.update(&world_);
    for(int i=0;i<8;++i) library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    EXPECT_EQ(library_.usage().retiredBytes,0);EXPECT_EQ(library_.pendingBytes(),0);
}

TEST_F(DesktopSceneTest, ReenterMapAtExactAnimationBudgetReusesRetiredGpuObjectsAndLatestFence) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::MapScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::MapScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    const auto path=std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath;
    std::weak_ptr<const cgl::MapResources> map=maps_.acquire(path,readers,device,pass);
    std::weak_ptr<const cgl::MapHoverOutline> outline=maps_.acquireHoverOutline(settings.display.ViewportScale,device,pass);
    std::weak_ptr<const cgl::AnimationClip> clip=library_.findReady({100000,0,0});
    const auto* mapAddress=map.lock().get();const auto* outlineAddress=outline.lock().get();const auto* clipAddress=clip.lock().get();
    const auto animationBytes=library_.usage().rgbaBytes;const auto mapBytes=maps_.usage().rgbaBytes;
    library_.setTextureBudget(animationBytes);
    draw_.update(&world_);draw_.update(&world_);
    const auto firstSerial=sync->progress.submitted;ASSERT_GT(firstSerial,0);
    maintenance_=[&] {
        library_.retireUnused(0,sync->progress.submitted);maps_.retireUnused(0,sync->progress.submitted,0);
        EXPECT_EQ(library_.usage().retiredBytes,animationBytes);EXPECT_EQ(maps_.usage().retiredMapBytes,mapBytes);
        EXPECT_FALSE(clip.expired());EXPECT_FALSE(map.expired());EXPECT_FALSE(outline.expired());
    };
    for(int round=1;round<=3;++round) {
        readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic]->resetCacheStatistics();
        scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        EXPECT_EQ(clip.lock().get(),clipAddress);EXPECT_EQ(map.lock().get(),mapAddress);EXPECT_EQ(outline.lock().get(),outlineAddress);
        EXPECT_EQ(library_.usage().rgbaBytes,animationBytes);EXPECT_EQ(library_.usage().retiredBytes,0);
        EXPECT_EQ(library_.pendingBytes(),0);EXPECT_EQ(maps_.usage().retiredMapBytes,0);EXPECT_EQ(maps_.usage().retiredOutlines,0);
        EXPECT_EQ(library_.usage().reusedClips,round*16);EXPECT_EQ(maps_.usage().reusedMaps,round);EXPECT_EQ(maps_.usage().reusedOutlines,round);
        EXPECT_EQ(readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic]->cacheStatistics().fileReadCalls,0);
        draw_.update(&world_);draw_.update(&world_);
        library_.collectRetired(sync->progress.completed);maps_.collectRetired(sync->progress.completed);
        EXPECT_FALSE(clip.expired());EXPECT_FALSE(map.expired());EXPECT_FALSE(outline.expired());
    }
    maintenance_={};cgl::SceneManagerDestroySystem{}.update(&world_);
    const auto latest=sync->progress.submitted;ASSERT_GT(latest,firstSerial);
    library_.retireUnused(0,latest);maps_.retireUnused(0,latest,0);
    EXPECT_EQ(library_.collectRetired(firstSerial),0);EXPECT_EQ(maps_.collectRetired(firstSerial),0);
    EXPECT_FALSE(clip.expired());EXPECT_FALSE(map.expired());EXPECT_FALSE(outline.expired());
    for(size_t i=0;i<sync->objs.size()+1;++i) draw_.update(&world_);
    ASSERT_GE(sync->progress.completed,latest);
    EXPECT_EQ(library_.collectRetired(sync->progress.completed),animationBytes);
    EXPECT_GT(maps_.collectRetired(sync->progress.completed),0);
    EXPECT_TRUE(clip.expired());EXPECT_TRUE(map.expired());EXPECT_TRUE(outline.expired());
}
TEST_F(DesktopSceneTest, ProgressiveReuseAtFullBudgetPreservesOtherPendingDecode) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while((!scene->scene || scene->scene->sceneType()!=cgl::SceneTypes::AnimationScene) && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(scene->scene,nullptr);ASSERT_EQ(scene->scene->sceneType(),cgl::SceneTypes::AnimationScene);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    const cgl::AnimationRequest request{100000,0,0},other{100000,1,0};
    auto pin=library_.acquire(request,readers,device,pass);std::weak_ptr<const cgl::AnimationClip> weak=pin;
    const auto* address=pin.get();
    const auto entity=world_.createEntity();world_.addComponent<cgl::component::Transform2D>(entity);
    world_.addComponent<cgl::component::Sprite>(entity,pin);world_.addComponent<cgl::component::AnimationPlayer>(entity);
    draw_.update(&world_);draw_.update(&world_);world_.destroyEntity(entity);pin.reset();
    const auto bytes=library_.usage().rgbaBytes;library_.setTextureBudget(bytes);
    ASSERT_GT(library_.retireUnused(0,sync->progress.submitted),0);
    EXPECT_THROW(library_.acquire(request,readers,nullptr,pass),std::invalid_argument);
    EXPECT_EQ(library_.usage().reusedClips,0);EXPECT_FALSE(weak.expired());
    EXPECT_EQ(library_.acquireProgressive(other,readers,device,pass),nullptr);ASSERT_TRUE(library_.waitingForBudget());
    auto restored=library_.acquireProgressive(request,readers,device,pass);
    ASSERT_NE(restored,nullptr);EXPECT_EQ(restored.get(),address);
    EXPECT_EQ(library_.usage().rgbaBytes,bytes);EXPECT_EQ(library_.usage().retiredBytes,0);
    EXPECT_EQ(library_.pendingBytes(),0);EXPECT_TRUE(library_.waitingForBudget());
    restored.reset();library_.setTextureBudget(bytes+32ull*1024*1024);
    EXPECT_EQ(library_.acquireProgressive(other,readers,device,pass),nullptr);
    const auto pending=library_.pendingBytes();ASSERT_GT(pending,0);
    library_.retireUnused(0,sync->progress.submitted);
    restored=library_.acquireProgressive(request,readers,device,pass);
    ASSERT_NE(restored,nullptr);EXPECT_EQ(restored.get(),address);EXPECT_EQ(library_.pendingBytes(),pending);
    EXPECT_EQ(library_.usage().reusedClips,2);EXPECT_EQ(library_.usage().retiredBytes,0);
    const auto usage=library_.usage();EXPECT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,usage.budgetBytes);
    library_.cancelPending();EXPECT_EQ(library_.pendingBytes(),0);
}

TEST_F(DesktopSceneTest, NewCharacterSceneWaitsForOldFramesWithoutPublishingPartialWorld) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    const auto catalog=cgl::ReadCharacterCatalog(readers,2);ASSERT_EQ(catalog.size(),2);
    const auto serial=catalog[1].serial;ASSERT_NE(serial,100000);
    cgl::AnimationBuilder planner;uint64_t required=0;
    for(uint32_t d=0;d<8;++d) for(uint32_t m=0;m<2;++m) required+=planner.requiredBytes({serial,d,m},readers);
    const auto oldBytes=library_.usage().rgbaBytes;library_.setTextureBudget(std::max(oldBytes,required));
    std::weak_ptr<const cgl::AnimationClip> old=library_.findReady({100000,0,0});
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    draw_.update(&world_);draw_.update(&world_);
    maintenance_=[&]{library_.retireUnused(0,sync->progress.submitted);maps_.retireUnused(0,sync->progress.submitted,0);};
    settings.animation.SerialNum=serial;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);
    EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->total,16);EXPECT_EQ(progress->requiredBytes,required);
    auto count=[&]{size_t n=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++n;});return n;};
    EXPECT_EQ(count(),0);EXPECT_FALSE(old.expired());EXPECT_EQ(library_.pendingBytes(),0);
    const auto center=world_.getSingleton<cgl::component::MapCamera>()->center;
    auto* input=world_.getSingleton<cgl::component::KeyboardInputState>();input->moveX=1;
    world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds=1;
    auto* gfx=readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get();gfx->resetCacheStatistics();
    for(int i=0;i<10;++i) manager_.update(&world_);
    EXPECT_EQ(gfx->cacheStatistics().fileReadCalls,0);EXPECT_EQ(count(),0);EXPECT_FALSE(progress->ready);
    EXPECT_EQ(library_.collectRetired(sync->progress.completed),0);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!progress->ready && std::chrono::steady_clock::now()<deadline) {
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);maps_.collectRetired(sync->progress.completed,1,2);
        manager_.update(&world_);draw_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        const auto usage=library_.usage();EXPECT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,usage.budgetBytes);
    }
    ASSERT_TRUE(progress->ready);EXPECT_EQ(progress->completed,16);EXPECT_EQ(count(),1);EXPECT_TRUE(old.expired());
    EXPECT_DOUBLE_EQ(world_.getSingleton<cgl::component::MapCamera>()->center.x,center.x);
    EXPECT_DOUBLE_EQ(world_.getSingleton<cgl::component::MapCamera>()->center.y,center.y);
    input->moveX=0;maintenance_={};
}
TEST_F(DesktopSceneTest, PartialSceneAdmissionCanRestartAndCancelWithoutLeakingPins) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto pin=library_.findReady({101406,0,5});ASSERT_NE(pin,nullptr);
    cgl::AnimationBuilder planner;uint64_t required=0,smallest=std::numeric_limits<uint64_t>::max();
    for(uint32_t d=0;d<8;++d) for(uint32_t m=0;m<2;++m) {
        const auto bytes=planner.requiredBytes({100000,d,m},readers);required+=bytes;smallest=std::min(smallest,bytes);
    }
    const auto oldBytes=library_.usage().rgbaBytes;
    library_.setTextureBudget(oldBytes+required-std::min(oldBytes,smallest)/2);
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();draw_.update(&world_);draw_.update(&world_);
    maintenance_=[&]{library_.retireUnused(0,sync->progress.submitted);maps_.retireUnused(0,sync->progress.submitted,0);};
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);
    ASSERT_FALSE(progress->ready);EXPECT_EQ(progress->completed,15);ASSERT_TRUE(library_.waitingForBudget());
    std::weak_ptr<const cgl::AnimationClip> partial=library_.findReady({100000,0,0});ASSERT_FALSE(partial.expired());
    const auto bytesBefore=library_.usage().rgbaBytes+library_.usage().retiredBytes;
    world_.getSingleton<cgl::component::KeyboardInputState>()->restartSceneRequested=true;
    manager_.update(&world_);manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    progress=world_.getSingleton<cgl::component::SceneLoadProgress>();EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->completed,15);
    EXPECT_EQ(library_.usage().rgbaBytes+library_.usage().retiredBytes,bytesBefore);EXPECT_GE(library_.usage().reusedClips,15);
    settings.animation.SerialNum=101406;settings.animation.Motion=5;
    scene->nextScene=cgl::SceneTypes::AnimationScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);EXPECT_TRUE(progress->ready);
    EXPECT_EQ(progress->total,1);EXPECT_FALSE(library_.waitingForBudget());EXPECT_EQ(library_.pendingBytes(),0);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapCamera>(),nullptr);EXPECT_EQ(world_.getSingleton<cgl::component::MapHover>(),nullptr);
    size_t sprites=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++sprites;});EXPECT_EQ(sprites,2);
    for(size_t i=0;i<sync->objs.size()+1;++i) draw_.update(&world_);
    library_.collectRetired(sync->progress.completed);maps_.collectRetired(sync->progress.completed);
    EXPECT_TRUE(partial.expired());EXPECT_NE(pin,nullptr);maintenance_={};
}
TEST_F(DesktopSceneTest, ImpossibleSceneWorkingSetFailsBeforeMapOrPixelAllocation) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(readers.state!=cgl::StateTypes::RUNNING && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(readers.state,cgl::StateTypes::RUNNING);ASSERT_EQ(library_.usage().clips,0);
    library_.setTextureBudget(1);
    auto* gfx=readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get();gfx->resetCacheStatistics();
    auto* scene=world_.getSingleton<cgl::component::SceneState>();scene->nextScene=cgl::SceneTypes::MapScene;manager_.update(&world_);
    EXPECT_EQ(scene->state,cgl::StateTypes::ERROR);EXPECT_NE(scene->lastError.find("working set"),std::string::npos);
    EXPECT_EQ(maps_.usage().maps,0);EXPECT_EQ(library_.usage().clips,0);EXPECT_EQ(library_.pendingBytes(),0);
    EXPECT_EQ(gfx->cacheStatistics().fileReadCalls,0);EXPECT_EQ(world_.getSingleton<cgl::component::MapCamera>(),nullptr);
    size_t sprites=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++sprites;});EXPECT_EQ(sprites,0);
}
TEST_F(DesktopSceneTest, PreviewSceneWaitsForRetiredCapacityThenPublishesBothSprites) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();cgl::AnimationBuilder planner;
    const auto nextBytes=planner.requiredBytes({100000,0,0},readers);const auto oldBytes=library_.usage().rgbaBytes;
    library_.setTextureBudget(std::max(oldBytes,nextBytes));
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();draw_.update(&world_);draw_.update(&world_);
    maintenance_=[&]{library_.retireUnused(0,sync->progress.submitted);};
    settings.animation.SerialNum=100000;settings.animation.Motion=0;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();scene->nextScene=cgl::SceneTypes::AnimationScene;manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);
    EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->completed,0);EXPECT_EQ(progress->total,1);
    size_t sprites=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++sprites;});EXPECT_EQ(sprites,0);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!progress->ready && std::chrono::steady_clock::now()<deadline) {
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);manager_.update(&world_);draw_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    }
    ASSERT_TRUE(progress->ready);sprites=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++sprites;});EXPECT_EQ(sprites,2);
    EXPECT_EQ(library_.pendingBytes(),0);maintenance_={};
}
