#include "map/streaming_stress.h"
#include "assets/map_library.h"
#include "assets/map_builder.h"
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
#include <cstdlib>
#include <iostream>
#include <string_view>
#include "vulkan/device.h"

namespace {
// Inject a late map-page failure while retaining the real decoder/ownership.
class MapDecodeFailure final:public cgl::IGraphicsDataReader {
public:
    MapDecodeFailure(Ptr source,uint32_t id):IGraphicsDataReader(source->createInfo()),source(std::move(source)),id(id) {}
    Ptr source;uint32_t id;size_t successful=0,failures=0;
    cgl::Results load() override {return source->load();}
    void close() noexcept override {source->close();}
    void setCacheBudget(size_t bytes) noexcept override {source->setCacheBudget(bytes);}
    void trimCache() noexcept override {source->trimCache();}
    cgl::ResourceCacheStatistics cacheStatistics() const noexcept override {return source->cacheStatistics();}
    void resetCacheStatistics() noexcept override {source->resetCacheStatistics();}
    cgl::Results query(const cgl::GraphicsResourceInfo& info,cgl::GraphicsResourceData* out) override {
        if(info.mapBasedsIdx.value==id) {++failures;return cgl::Results::Fail;}
        const auto result=source->query(info,out);if(result==cgl::Results::Success) ++successful;return result;
    }
};
// Enlarge only the transparent canvas; the real encoded image and dimensions
// are preserved, exercising >32 MiB local admission with installed assets.
class LargeCanvasInfo final:public cgl::IGraphicsInfoReader {
public:
    explicit LargeCanvasInfo(Ptr source):IGraphicsInfoReader(source->createInfo()),source(std::move(source)) {}
    Ptr source;
    cgl::Results load() override {return source->load();}
    void close() noexcept override {source->close();}
    size_t infoCount() const noexcept override {return source->infoCount();}
    bool mightContain(const cgl::GraphicsResourceSerialNum& key) const noexcept override {return source->mightContain(key);}
    cgl::Results queryAvailableSerialNums(std::vector<int32_t>* out) noexcept override {return source->queryAvailableSerialNums(out);}
    cgl::Results query(const cgl::GraphicsResourceSerialNum& key,cgl::GraphicsResourceInfo* out) const noexcept override {
        const auto result=source->query(key,out);
        if(result==cgl::Results::Success) {out->offsetX+=2048;out->offsetY+=2048;}
        return result;
    }
};
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
        EXPECT_EQ(static_cast<cgl::graphics::vulkan::Device*>(render.pDevice.get())->validationErrorCount(),0);
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
            // Check through device destruction while the diagnostic owner lives.
            render->pSwapchain.reset();
            auto* native=static_cast<cgl::graphics::vulkan::Device*>(render->pDevice.get());
            native->destroy();
            EXPECT_EQ(native->validationErrorCount(),0);
        }
        if (render) cgl::RenderDeviceDestroySystem{}.update(&world_);
        if (auto* window = world_.getSingleton<cgl::component::WindowHandle>())
            glfwDestroyWindow(static_cast<GLFWwindow*>(window->nativeHandle));
        glfwTerminate();
    }
    template<class Predicate> bool waitWithoutDrawing(Predicate&& ready) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(!ready() && std::chrono::steady_clock::now()<deadline) {
            manager_.update(&world_);
            if(world_.getSingleton<cgl::component::SceneState>()->state==cgl::StateTypes::ERROR) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return ready();
    }
    bool waitMapPrepared() {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(!world_.getSingleton<cgl::component::MapCamera>() && std::chrono::steady_clock::now()<deadline) {
            manager_.update(&world_);
            if(world_.getSingleton<cgl::component::SceneState>()->state==cgl::StateTypes::ERROR) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return world_.getSingleton<cgl::component::MapCamera>()!=nullptr;
    }
    bool mapReady() {
        const auto* scene=world_.getSingleton<cgl::component::SceneState>();
        const auto* loading=world_.getSingleton<cgl::component::SceneLoadProgress>();
        return scene->scene && scene->scene->sceneType()==cgl::SceneTypes::MapScene &&
            world_.getSingleton<cgl::component::MapCamera>() && (!loading || loading->ready);
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
    while (!mapReady() &&
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
    while (!mapReady() &&
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
    while (!mapReady() &&
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
    for(int i=0;i<20 && !library_.streamingPending();++i) manager_.update(&world_);
    EXPECT_TRUE(library_.streamingPending()); // Metadata can occupy the slot with zero RGBA.
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
    ASSERT_TRUE(progressive.cpuPending());EXPECT_LE(progressive.pendingBytes(),32*1024*1024);
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
    ASSERT_TRUE(progressive.cpuPending());
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
    while (!mapReady() && std::chrono::steady_clock::now()<deadline) {
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
        ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
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
    while (!mapReady() && std::chrono::steady_clock::now()<deadline) {
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
    EXPECT_EQ(world_.getSingleton<cgl::component::RuntimeTelemetry>(),nullptr);
    EXPECT_EQ(world_.getSingleton<cgl::component::StreamingDiagnostics>()->demand.summary().started,0);
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
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
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
    EXPECT_TRUE(cache.cpuPending());EXPECT_EQ(cache.trimUnused(0),0);EXPECT_TRUE(cache.cpuPending());
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
    while (!mapReady() && std::chrono::steady_clock::now()<deadline) {
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
        ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
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
    while (!mapReady() && std::chrono::steady_clock::now()<deadline) {
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
        ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
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
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(remote.budgetDeferred,1); // No active request; owner must still expire.
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
    while (!mapReady() && std::chrono::steady_clock::now()<deadline) {
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
    while(!library_.waitingForBudget() && std::chrono::steady_clock::now()<deadline) {
        EXPECT_EQ(library_.acquireProgressive(other,readers,device,pass),nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(library_.waitingForBudget());
    auto restored=library_.acquireProgressive(request,readers,device,pass);
    ASSERT_NE(restored,nullptr);EXPECT_EQ(restored.get(),address);
    EXPECT_EQ(library_.usage().rgbaBytes,bytes);EXPECT_EQ(library_.usage().retiredBytes,0);
    EXPECT_EQ(library_.pendingBytes(),0);EXPECT_TRUE(library_.waitingForBudget());
    restored.reset();library_.setTextureBudget(bytes+32ull*1024*1024);
    EXPECT_EQ(library_.acquireProgressive(other,readers,device,pass),nullptr);
    while(!library_.pendingBytes() && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    ASSERT_TRUE(waitMapPrepared());
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);
    EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->total,16);EXPECT_EQ(progress->requiredBytes,required);
    auto count=[&]{size_t n=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++n;});return n;};
    ASSERT_TRUE(waitWithoutDrawing([&]{return library_.waitingForBudget() && !library_.streamingPending();}));
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
    ASSERT_TRUE(waitMapPrepared());
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();ASSERT_NE(progress,nullptr);
    ASSERT_TRUE(waitWithoutDrawing([&]{return library_.waitingForBudget() && !library_.streamingPending();}));
    ASSERT_FALSE(progress->ready);EXPECT_EQ(progress->completed,15);ASSERT_TRUE(library_.waitingForBudget());
    std::weak_ptr<const cgl::AnimationClip> partial=library_.findReady({100000,0,0});ASSERT_FALSE(partial.expired());
    const auto bytesBefore=library_.usage().rgbaBytes+library_.usage().retiredBytes;
    world_.getSingleton<cgl::component::KeyboardInputState>()->restartSceneRequested=true;
    manager_.update(&world_);manager_.update(&world_);
    ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
    ASSERT_TRUE(waitWithoutDrawing([&]{return library_.waitingForBudget() && !library_.streamingPending();}));
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

TEST_F(DesktopSceneTest, RemoteVisibleDemandDiscardsPrefetchCpuWorkBeforeAnyUpload) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest prefetch{100000,0,0},visible{100000,1,0};
    cgl::RemoteCharacters remote({prefetch,visible});
    cgl::component::MapCamera camera;camera.center={10,10};const cgl::Viewport viewport{0,0,800,600};
    const auto outer=cgl::ScreenToMap({-100,300},camera,viewport);
    remote.apply(world_,{1,{{1,outer,0}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    ASSERT_TRUE(library_.cpuPending());ASSERT_FALSE(library_.uploadPending());
    remote.apply(world_,{2,{{1,outer,0},{2,{10,10},1}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(library_.discardedDecodes(),1);EXPECT_EQ(library_.findReady(prefetch),nullptr);
    // Remove prefetch interest. The visible appearance must finish without stale upload.
    remote.apply(world_,{3,{{2,{10,10},1}}},placeholder);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!remote.readyVisible && std::chrono::steady_clock::now()<deadline) {
        library_.maintainStreaming(0,0);remote.update(world_,library_,readers,device,pass,camera,viewport);
        const auto u=library_.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,u.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(remote.readyVisible,1);EXPECT_NE(library_.findReady(visible),nullptr);EXPECT_EQ(library_.findReady(prefetch),nullptr);
    // Repeated complete replacement before upload must leave no queued work or pins.
    for(uint64_t i=4;i<36;++i) {
        remote.apply(world_,{i,{{i,{10,10},0}}},placeholder);
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        remote.apply(world_,{++i,{}},placeholder);
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        while(library_.streamingPending() && std::chrono::steady_clock::now()<deadline) {
            library_.maintainStreaming(0,0);std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_EQ(library_.pendingBytes(),0);
    }
    while(library_.streamingPending() && std::chrono::steady_clock::now()<deadline) {
        library_.maintainStreaming(0,0);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(library_.pendingBytes(),0);EXPECT_EQ(library_.findReady(prefetch),nullptr);
    EXPECT_GE(library_.discardedDecodes(),17);EXPECT_FALSE(library_.waitingForBudget());remote.clear(world_);
}
TEST_F(DesktopSceneTest, NonblockingDiscardNeverCancelsSubmittedGpuUpload) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    cgl::AnimationLibrary cache;const cgl::AnimationRequest request{100000,0,0};
    EXPECT_EQ(cache.acquireProgressive(request,readers,device,pass,false),nullptr);
    ASSERT_TRUE(cache.uploadPending());const auto bytes=cache.pendingBytes();ASSERT_GT(bytes,0);
    EXPECT_FALSE(cache.discardUnsubmitted());EXPECT_TRUE(cache.uploadPending());EXPECT_EQ(cache.pendingBytes(),bytes);
    EXPECT_EQ(cache.discardedDecodes(),0);
    std::shared_ptr<const cgl::AnimationClip> clip;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!clip && std::chrono::steady_clock::now()<deadline) {
        clip=cache.acquireProgressive(request,readers,device,pass,false);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(clip,nullptr);EXPECT_EQ(cache.pendingBytes(),0);EXPECT_EQ(cache.usage().clips,1);
    device->waitIdle();clip.reset();cache.clear();EXPECT_EQ(cache.usage().clips,0);
}

TEST_F(DesktopSceneTest, BackgroundMetadataKeepsZeroByteSlotAndCancelsWithoutOwnerWait) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    cgl::AnimationLibrary cache;
    {
        // The owner holds reader access; background metadata must not block it.
        std::unique_lock held(*readers.queryMutex);
        EXPECT_EQ(cache.acquireProgressive({100000,0,0},readers,device,pass),nullptr);
        ASSERT_TRUE(cache.cpuPending());EXPECT_EQ(cache.pendingBytes(),0);
        EXPECT_TRUE(cache.loading({100000,0,0}));EXPECT_FALSE(cache.loading({100000,1,0}));
        EXPECT_EQ(cache.acquireProgressive({100000,1,0},readers,device,pass),nullptr);
        EXPECT_THROW(cache.setTextureBudget(cache.usage().budgetBytes-1),std::invalid_argument);
        EXPECT_TRUE(cache.discardUnsubmitted());EXPECT_EQ(cache.pendingBytes(),0);
        EXPECT_FALSE(cache.loading({100000,0,0})); // Draining cancellation is not another request's service time.
        EXPECT_EQ(cache.loadTimings().completed,0);EXPECT_EQ(cache.cancelledLoads(),1);
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(cache.cpuPending() && std::chrono::steady_clock::now()<deadline) {
        cache.maintainStreaming(0,0);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_FALSE(cache.cpuPending());EXPECT_EQ(cache.pendingBytes(),0);EXPECT_EQ(cache.usage().clips,0);EXPECT_EQ(cache.failedLoads(),0);
}
TEST_F(DesktopSceneTest, BackgroundDecodeReservesExactRgbaAndRecordsOnlySuccessfulReadiness) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest request{100000,0,0};cgl::AnimationBuilder planner;
    const auto required=planner.requiredBytes(request,readers);ASSERT_LT(required,cgl::AnimationDecodeWorker::MaxBytes);
    cgl::AnimationLibrary cache;cache.setTextureBudget(required);
    EXPECT_EQ(cache.acquireProgressive(request,readers,device,pass),nullptr);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!cache.pendingBytes() && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(cache.cpuPending());ASSERT_EQ(cache.pendingBytes(),required);EXPECT_FALSE(cache.uploadPending());
    EXPECT_EQ(cache.loadTimings().completed,0);
    std::shared_ptr<const cgl::AnimationClip> clip;
    while(!clip && std::chrono::steady_clock::now()<deadline) {
        clip=cache.acquireProgressive(request,readers,device,pass);
        EXPECT_LE(cache.usage().rgbaBytes+cache.pendingBytes()+cache.usage().retiredBytes,required);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(clip,nullptr);EXPECT_EQ(cache.pendingBytes(),0);EXPECT_FALSE(cache.cpuPending());
    EXPECT_EQ(cache.usage().rgbaBytes,required);const auto times=cache.loadTimings();
    EXPECT_EQ(times.completed,1);EXPECT_EQ(times.count,1);EXPECT_GT(times.meanMs,0);
    EXPECT_GE(times.meanMs,times.cpuMeanMs);EXPECT_GT(times.uploadWaitMeanMs,0);
    EXPECT_EQ(cache.acquireProgressive(request,readers,device,pass),clip);EXPECT_EQ(cache.loadTimings().completed,1);
    EXPECT_EQ(cache.cancelledLoads(),0);EXPECT_EQ(cache.failedLoads(),0);
    cache.cancelPending();EXPECT_EQ(cache.cancelledLoads(),0); // No active load to cancel.
    device->waitIdle();clip.reset();cache.clear();EXPECT_EQ(cache.loadTimings().completed,0);
}
TEST_F(DesktopSceneTest, BackgroundInsufficientTotalBudgetFailsBeforeAnyPixelRead) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto version=cgl::CrossGateVersion::CG_VERSION_Classic;
    const auto before=readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls;
    cgl::AnimationLibrary cache;cache.setTextureBudget(1);bool failed=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!failed && std::chrono::steady_clock::now()<deadline) {
        try {EXPECT_EQ(cache.acquireProgressive({100000,0,0},readers,device,pass),nullptr);}
        catch(const std::runtime_error&) {failed=true;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(failed);EXPECT_EQ(cache.failedLoads(),1);EXPECT_EQ(cache.cancelledLoads(),0);
    EXPECT_EQ(cache.loadTimings().completed,0);EXPECT_EQ(cache.pendingBytes(),0);EXPECT_FALSE(cache.cpuPending());
    EXPECT_EQ(cache.usage().clips,0);EXPECT_FALSE(cache.waitingForBudget());
    EXPECT_EQ(readers.pGfxDataReader[version]->cacheStatistics().fileReadCalls,before);
    cache.cancelPending();EXPECT_EQ(cache.cancelledLoads(),0);
}

TEST_F(DesktopSceneTest, ScriptedServerChurnMovesAndRestartsWithoutLeakingEntities) {
    world_.addSingleton<cgl::component::RuntimeTelemetry>();
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto& demo=settings.serverdemo;demo.Enabled=true;demo.AnimationBudgetMiB=16;
    demo.VisibleActors=8;demo.PrefetchActors=8;demo.CatalogLimit=32;demo.Seed=42;
    demo.StressFrames=96;demo.StressLegFrames=24;demo.StressReplaceFrames=16;demo.StressStepMs=20;
    library_.setTextureBudget(16ull*1024*1024);
    auto* window=world_.getSingleton<cgl::component::WindowHandle>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    uint64_t fingerprint=0;double travel=0;
    for(int epoch=0;epoch<2;++epoch) {
        window->windowShouldClose=false;
        if(epoch) world_.getSingleton<cgl::component::SceneState>()->nextScene=cgl::SceneTypes::MapScene;
        ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
        auto* stats=world_.getSingleton<cgl::component::StreamingDiagnostics>();ASSERT_NE(stats,nullptr);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        while(stats->frames<demo.StressFrames && std::chrono::steady_clock::now()<deadline) {
            world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds=1; // Script must override real delta/input.
            world_.getSingleton<cgl::component::KeyboardInputState>()->moveY=-1;
            library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
            manager_.update(&world_);draw_.update(&world_);
            ASSERT_NE(world_.getSingleton<cgl::component::SceneState>()->state,cgl::StateTypes::ERROR);
        }
        ASSERT_EQ(stats->frames,96);EXPECT_TRUE(window->windowShouldClose);
        EXPECT_EQ(stats->replacements,7);EXPECT_GE(stats->snapshots,7);
        EXPECT_GT(stats->travelCells,7);EXPECT_LT(stats->travelCells,8);EXPECT_LE(stats->peakAccountedBytes,16ull*1024*1024);
        EXPECT_LE(stats->readyVisibleSamples,stats->visibleSamples);EXPECT_GT(stats->visibleSamples,0);
        size_t count=0;world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++count;});EXPECT_EQ(count,16);
        if(!epoch) {
            EXPECT_GT(stats->demand.summary().started,0); // First cold run must observe misses.
            fingerprint=stats->fingerprint;travel=stats->travelCells;
        }
        else {EXPECT_EQ(stats->fingerprint,fingerprint);EXPECT_DOUBLE_EQ(stats->travelCells,travel);}
        device->waitIdle();
    }
    const auto latency=world_.getSingleton<cgl::component::StreamingDiagnostics>()->demand.summary();
    EXPECT_LE(latency.pending,16); // A warm restart may legitimately have no misses.
    EXPECT_EQ(latency.started,latency.completed+latency.withdrawn+latency.failed+latency.pending);
    std::weak_ptr<const cgl::AnimationClip> player=library_.findReady({100000,0,0});EXPECT_FALSE(player.expired());
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(world_.getSingleton<cgl::component::StreamingDiagnostics>(),nullptr);
    size_t count=0;world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++count;});EXPECT_EQ(count,0);
    EXPECT_FALSE(library_.cpuPending());EXPECT_EQ(library_.pendingBytes(),0);library_.clear();EXPECT_TRUE(player.expired());
}

TEST_F(DesktopSceneTest, VisibleWorkingSetOverBudgetKeepsEntitiesAndPlaceholders) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    auto catalog=cgl::ReadCharacterCatalog(readers,4);ASSERT_EQ(catalog.size(),4);
    cgl::AnimationBuilder planner;uint64_t largest=0,total=0;
    for(const auto& key:catalog) {const auto bytes=planner.requiredBytes(key,readers);largest=std::max(largest,bytes);total+=bytes;}
    ASSERT_GT(total,largest);const auto baseline=library_.usage().rgbaBytes;library_.setTextureBudget(baseline+largest);
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    cgl::RemoteCharacters remote(catalog);cgl::component::MapCamera camera{{10,10},1};const cgl::Viewport viewport{0,0,800,600};
    remote.apply(world_,{1,{{1,{10,10},0},{2,{10,10},1},{3,{10,10},2},{4,{10,10},3}}},placeholder);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!library_.waitingForBudget() && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);draw_.update(&world_);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(library_.waitingForBudget());ASSERT_GT(remote.readyVisible,0);ASSERT_LT(remote.readyVisible,4);
    for(int i=0;i<64;++i) {
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
        remote.update(world_,library_,readers,device,pass,camera,viewport);draw_.update(&world_);
        const auto u=library_.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,u.budgetBytes);
    }
    size_t count=0,placeholders=0,deferred=0;
    world_.forEach<cgl::component::RemoteCharacter,cgl::component::Sprite>([&](auto,auto* tag,auto* sprite) {
        ++count;ASSERT_NE(sprite->clip,nullptr);
        if(tag->state==cgl::component::AppearanceState::Deferred) ++deferred;
        if(sprite->clip==placeholder) {
            ++placeholders;EXPECT_FALSE(tag->presented);
            EXPECT_NE(tag->state,cgl::component::AppearanceState::Ready);
            EXPECT_NE(tag->state,cgl::component::AppearanceState::Failed);
        } else {
            EXPECT_TRUE(tag->presented);EXPECT_EQ(tag->state,cgl::component::AppearanceState::Ready);
        }
    });
    EXPECT_EQ(count,4);EXPECT_GT(placeholders,0);EXPECT_EQ(remote.visible,4);EXPECT_LT(remote.readyVisible,4);
    EXPECT_GT(deferred,0);
    EXPECT_EQ(library_.failedLoads(),0); // Temporary pressure must not poison appearances.
    remote.clear(world_);library_.cancelPending();EXPECT_FALSE(library_.cpuPending());
    EXPECT_GT(library_.trimUnused(baseline),0);EXPECT_EQ(library_.usage().rgbaBytes,baseline);
}

TEST_F(DesktopSceneTest, PrefetchDoesNotPinTexturesAndVisiblePromotionSurvivesPressure) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    const auto baseline=library_.usage().rgbaBytes;
    const cgl::AnimationRequest prefetched{100000,0,0},incoming{100000,1,0};
    cgl::AnimationBuilder planner;
    const auto firstBytes=planner.requiredBytes(prefetched,readers);
    const auto secondBytes=planner.requiredBytes(incoming,readers);
    library_.setTextureBudget(baseline+std::max(firstBytes,secondBytes));
    std::weak_ptr<const cgl::AnimationClip> old=library_.acquire(prefetched,readers,device,pass);
    ASSERT_FALSE(old.expired());
    cgl::RemoteCharacters remote({prefetched,incoming});
    cgl::component::MapCamera camera{{10,10},1};const cgl::Viewport viewport{0,0,800,600};
    const auto outside=cgl::ScreenToMap({-100,300},camera,viewport);
    remote.apply(world_,{1,{{1,{10,10},0},{2,outside,0}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    ASSERT_EQ(remote.readyVisible,1);ASSERT_EQ(remote.readyOuter,1);
    // Shared outer/visible key stays alive through the visible owner.
    library_.retireUnused(baseline,sync->progress.submitted);
    EXPECT_NE(library_.findReady(prefetched),nullptr);
    // A foot outside the viewport must not discard an image still on-screen.
    auto cached=old.lock();ASSERT_NE(cached,nullptr);ASSERT_LT(cached->offsetY,0);
    const auto edge=cgl::ScreenToMap({400,600-cached->offsetY*.5},camera,viewport);
    remote.apply(world_,{2,{{1,edge,0},{2,outside,0}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    world_.forEach<cgl::component::RemoteCharacter,cgl::component::Sprite>([&](auto,auto* tag,auto* sprite) {
        if(tag->serverId==1) EXPECT_EQ(sprite->clip,cached);
    });
    cached.reset();
    // Both old owners become fully off-screen; the new visible clip needs their bytes.
    remote.apply(world_,{3,{{1,outside,0},{2,outside,0},{3,{10,10},1}}},placeholder);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while((!remote.readyVisible || !library_.findReady(incoming)) && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);draw_.update(&world_);
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
        const auto u=library_.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,u.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(remote.readyVisible,1);ASSERT_NE(library_.findReady(incoming),nullptr);
    EXPECT_TRUE(old.expired());EXPECT_GT(library_.usage().evictions,0);
    world_.forEach<cgl::component::RemoteCharacter,cgl::component::Sprite>([&](auto,auto* tag,auto* sprite) {
        if(tag->serverId<3) EXPECT_EQ(sprite->clip,placeholder);
        else EXPECT_EQ(sprite->clip,library_.findReady(incoming));
    });
    // Promoting the cached visible key to another actor does not duplicate it.
    remote.apply(world_,{4,{{4,{10,10},1}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(remote.readyVisible,1);EXPECT_EQ(library_.failedLoads(),0);
    remote.clear(world_);EXPECT_EQ(remote.visible,0);EXPECT_EQ(remote.outer,0);
    library_.cancelPending();device->waitIdle();library_.collectRetired(UINT64_MAX);
    library_.trimUnused(baseline);EXPECT_EQ(library_.usage().rgbaBytes,baseline);
}

TEST_F(DesktopSceneTest, BudgetBlockedVisibleClipDoesNotStarveSmallerVisibleClip) {
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=101406;settings.animation.Direction=0;settings.animation.Motion=5;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    const auto baseline=library_.usage().rgbaBytes;
    auto source=cgl::ReadCharacterCatalog(readers,32);ASSERT_EQ(source.size(),32);
    cgl::AnimationBuilder planner;
    std::vector<std::pair<uint64_t,cgl::AnimationRequest>> sizes;
    for(const auto& request:source) sizes.emplace_back(planner.requiredBytes(request,readers),request);
    std::stable_sort(sizes.begin(),sizes.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    const auto small=sizes.front(),large=sizes[sizes.size()-2],resident=sizes.back();
    ASSERT_GT(large.first,small.first);
    library_.setTextureBudget(baseline+resident.first+small.first);
    std::weak_ptr<const cgl::AnimationClip> residentWeak=library_.acquire(resident.second,readers,device,pass);
    cgl::AppearanceDemandTelemetry demand;
    cgl::RemoteCharacters remote({resident.second,large.second,small.second});
    cgl::component::MapCamera camera{{10,10},1};const cgl::Viewport viewport{0,0,800,600};
    const auto far=cgl::ScreenToMap({150,300},camera,viewport);
    remote.apply(world_,{1,{{1,far,0}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport,&demand);ASSERT_EQ(remote.readyVisible,1);
    EXPECT_EQ(library_.eventualAdmissionBytes(),small.first);
    remote.apply(world_,{2,{{1,far,0},{2,{10,10},1},{3,far,2}}},placeholder);
    bool sawWait=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!library_.findReady(small.second) && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport,&demand);
        sawWait|=library_.waitingForBudget();draw_.update(&world_);
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
        const auto u=library_.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,u.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(sawWait);ASSERT_NE(library_.findReady(small.second),nullptr);
    EXPECT_EQ(library_.findReady(large.second),nullptr);EXPECT_FALSE(residentWeak.expired());
    for(int i=0;i<64;++i) {
        remote.update(world_,library_,readers,device,pass,camera,viewport,&demand);draw_.update(&world_);
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
    }
    EXPECT_EQ(remote.readyVisible,2);EXPECT_EQ(library_.usage().evictions,0);
    EXPECT_EQ(remote.budgetDeferred,1);EXPECT_FALSE(library_.streamingPending());
    EXPECT_EQ(library_.failedLoads(),0);EXPECT_EQ(library_.eventualAdmissionBytes(),0);
    // Releasing a resident changes eventual capacity; blocked demand retries,
    // but its actual allocation still waits for retirement and frame completion.
    remote.apply(world_,{3,{{1,cgl::ScreenToMap({-10000,300},camera,viewport),0},{2,{10,10},1},{3,far,2}}},placeholder);
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!library_.findReady(large.second) && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport,&demand);draw_.update(&world_);
        library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
        const auto u=library_.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,u.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(library_.findReady(large.second),nullptr);EXPECT_TRUE(residentWeak.expired());
    remote.update(world_,library_,readers,device,pass,camera,viewport,&demand);
    EXPECT_EQ(remote.readyVisible,2);EXPECT_EQ(library_.failedLoads(),0);
    const auto latency=demand.summary();EXPECT_EQ(latency.completed,2);EXPECT_EQ(latency.pending,0);
    EXPECT_EQ(latency.started,latency.completed+latency.withdrawn+latency.failed+latency.pending);
    EXPECT_GT(latency.deferredMeanMs,0);
    EXPECT_NEAR(latency.meanMs,latency.queuedMeanMs+latency.deferredMeanMs+latency.loadingMeanMs,1e-6);
    remote.clear(world_);EXPECT_EQ(remote.budgetDeferred,0);library_.cancelPending();device->waitIdle();library_.collectRetired(UINT64_MAX);
    library_.trimUnused(baseline);EXPECT_EQ(library_.usage().rgbaBytes,baseline);
}

TEST_F(DesktopSceneTest, PreparedDecodeOverlapsUploadWithinCombinedReservation) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest first{100000,0,0},next{100000,1,0};
    cgl::AnimationBuilder planner;
    const auto a=planner.requiredBytes(first,readers),b=planner.requiredBytes(next,readers);
    ASSERT_LE(a+b,cgl::AnimationDecodeWorker::MaxBytes);
    cgl::AnimationLibrary cache;cache.setTextureBudget(a+b);
    EXPECT_FALSE(cache.prepareNext(next,readers)); // Only overlap an existing upload.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!cache.uploadPending() && std::chrono::steady_clock::now()<deadline) {
        cache.acquireProgressive(first,readers,device,pass);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(cache.uploadPending());ASSERT_TRUE(cache.prepareNext(next,readers));
    EXPECT_FALSE(cache.prepareNext(first,readers));EXPECT_FALSE(cache.prepareNext(next,readers));
    while(cache.pendingBytes()!=a+b && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ASSERT_TRUE(cache.cpuPending());ASSERT_EQ(cache.pendingBytes(),a+b);
    EXPECT_TRUE(cache.loading(first));EXPECT_TRUE(cache.loading(next));
    EXPECT_THROW(cache.setTextureBudget(a+b-1),std::invalid_argument);
    EXPECT_FALSE(cache.discardUnsubmitted()); // Submitted upload survives.
    std::shared_ptr<const cgl::AnimationClip> one,two;
    while(!two && std::chrono::steady_clock::now()<deadline) {
        if(!one) one=cache.acquireProgressive(first,readers,device,pass);
        else two=cache.acquireProgressive(next,readers,device,pass);
        const auto u=cache.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,a+b);
        EXPECT_LE(u.pendingBytes,cgl::AnimationDecodeWorker::MaxBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(one,nullptr);ASSERT_NE(two,nullptr);EXPECT_NE(one,two);
    EXPECT_EQ(cache.loadTimings().completed,2);
    EXPECT_GE(cache.loadTimings().meanMs,cache.loadTimings().cpuMeanMs);
    EXPECT_EQ(cache.failedLoads(),0);EXPECT_EQ(cache.cancelledLoads(),0);
    EXPECT_FALSE(cache.streamingPending());EXPECT_EQ(cache.pendingBytes(),0);
    device->waitIdle();one.reset();two.reset();cache.clear();
    EXPECT_EQ(cache.usage().rgbaBytes,0);EXPECT_EQ(cache.pendingBytes(),0);
}
TEST_F(DesktopSceneTest, CancellingPreparedMetadataNeverBlocksOrCancelsUpload) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest first{100000,0,0},next{100000,1,0};
    cgl::AnimationLibrary cache;cache.acquireProgressive(first,readers,device,pass,false);
    const auto bytes=cache.pendingBytes();ASSERT_GT(bytes,0);
    cache.setTextureBudget(bytes);EXPECT_FALSE(cache.prepareNext(next,readers)); // No free budget.
    cache.setTextureBudget(bytes+32ull*1024*1024);
    std::shared_ptr<const cgl::AnimationClip> one;
    {
        std::unique_lock held(*readers.queryMutex);
        ASSERT_TRUE(cache.prepareNext(next,readers));EXPECT_EQ(cache.pendingBytes(),bytes);
        cache.discardPrepared();EXPECT_FALSE(cache.loading(next));EXPECT_TRUE(cache.uploadPending());
        EXPECT_FALSE(cache.discardUnsubmitted());
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
        while(!one && std::chrono::steady_clock::now()<deadline) {
            one=cache.acquireProgressive(first,readers,device,pass);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        EXPECT_NE(one,nullptr); // Progress continues even if cancelled CPU access is draining.
    }
    cache.cancelPending();EXPECT_FALSE(cache.streamingPending());EXPECT_EQ(cache.pendingBytes(),0);
    EXPECT_EQ(cache.cancelledLoads(),1);EXPECT_EQ(cache.discardedDecodes(),1);EXPECT_EQ(cache.failedLoads(),0);
    EXPECT_EQ(cache.findReady(next),nullptr);
    // Reuse after cancellation must not inherit its timing, result or reader plan.
    std::shared_ptr<const cgl::AnimationClip> two;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!two && std::chrono::steady_clock::now()<deadline) {
        two=cache.acquireProgressive(next,readers,device,pass);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(two,nullptr);EXPECT_EQ(cache.loadTimings().completed,1);
    device->waitIdle();one.reset();two.reset();cache.clear();EXPECT_EQ(cache.pendingBytes(),0);
}

TEST_F(DesktopSceneTest, ReplacedSnapshotDiscardsLookaheadButFinishesSubmittedAppearance) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    const cgl::AnimationRequest first{100000,0,0},next{100000,1,0};
    cgl::RemoteCharacters remote({first,next});
    cgl::component::MapCamera camera;camera.center={10,10};const cgl::Viewport viewport{0,0,800,600};
    remote.apply(world_,{1,{{1,{10,10},0},{2,{11,10},1}}},placeholder);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!library_.uploadPending() && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(library_.uploadPending());ASSERT_TRUE(library_.cpuPending());
    ASSERT_TRUE(library_.loading(first));ASSERT_TRUE(library_.loading(next));
    remote.apply(world_,{2,{}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_FALSE(library_.loading(next));EXPECT_EQ(library_.discardedDecodes(),1);
    while(library_.streamingPending() && std::chrono::steady_clock::now()<deadline) {
        library_.maintainStreaming(0,0);remote.update(world_,library_,readers,device,pass,camera,viewport);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_FALSE(library_.streamingPending());EXPECT_NE(library_.findReady(first),nullptr);
    EXPECT_EQ(library_.findReady(next),nullptr);EXPECT_EQ(library_.pendingBytes(),0);
    EXPECT_EQ(library_.failedLoads(),0);EXPECT_EQ(library_.cancelledLoads(),1);
    remote.clear(world_);
}

TEST_F(DesktopSceneTest, PreparedAdmissionMissPreservesUploadAndRecoversWithExactBudget) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const cgl::AnimationRequest first{100000,0,0},next{100000,1,0};
    cgl::AnimationBuilder planner;
    const auto a=planner.requiredBytes(first,readers),b=planner.requiredBytes(next,readers);
    cgl::AnimationLibrary cache;cache.setTextureBudget(a+b-1);
    cache.acquireProgressive(first,readers,device,pass,false);
    ASSERT_TRUE(cache.prepareNext(next,readers));
    std::shared_ptr<const cgl::AnimationClip> one,two;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(!cache.waitingForBudget() && std::chrono::steady_clock::now()<deadline) {
        if(!one) one=cache.acquireProgressive(first,readers,device,pass);
        else two=cache.acquireProgressive(next,readers,device,pass);
        const auto u=cache.usage();EXPECT_LE(u.rgbaBytes+u.retiredBytes+u.pendingBytes,a+b-1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(one,nullptr);EXPECT_EQ(two,nullptr);ASSERT_EQ(cache.waitingBytes(next),b);
    EXPECT_FALSE(cache.streamingPending());EXPECT_EQ(cache.failedLoads(),0);EXPECT_EQ(cache.pendingBytes(),0);
    cache.setTextureBudget(a+b);
    while(!two && std::chrono::steady_clock::now()<deadline) {
        two=cache.acquireProgressive(next,readers,device,pass);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(two,nullptr);EXPECT_EQ(cache.failedLoads(),0);EXPECT_EQ(cache.cancelledLoads(),0);
    EXPECT_EQ(cache.loadTimings().completed,1);EXPECT_EQ(cache.usage().rgbaBytes,a+b);
    device->waitIdle();one.reset();two.reset();cache.clear();EXPECT_EQ(cache.pendingBytes(),0);
}


TEST_F(DesktopSceneTest, RemoteAppearanceStateSurvivesFailureAndSnapshotReplacementWithoutTelemetry) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto placeholder=library_.findReady({101406,0,5});ASSERT_NE(placeholder,nullptr);
    cgl::RemoteCharacters remote({{100000,0,0},{UINT32_MAX,0,0},{101406,0,5}});
    cgl::component::MapCamera camera{{10,10},1};const cgl::Viewport viewport{0,0,800,600};
    remote.apply(world_,{1,{{1,{10,10},0}}},placeholder);
    cgl::component::RemoteCharacter* tag=nullptr;
    world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto* value){tag=value;});
    ASSERT_NE(tag,nullptr);EXPECT_EQ(tag->state,cgl::component::AppearanceState::Queued);EXPECT_FALSE(tag->presented);
    {
        std::unique_lock held(*readers.queryMutex);
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        EXPECT_EQ(tag->state,cgl::component::AppearanceState::Loading);EXPECT_FALSE(tag->presented);
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(tag->state!=cgl::component::AppearanceState::Ready && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(tag->state,cgl::component::AppearanceState::Ready);EXPECT_TRUE(tag->presented);EXPECT_EQ(remote.readyVisible,1);
    remote.apply(world_,{2,{{1,{10,10},1}}},placeholder);
    EXPECT_EQ(tag->state,cgl::component::AppearanceState::Queued);EXPECT_FALSE(tag->presented);
    while(tag->state!=cgl::component::AppearanceState::Failed && std::chrono::steady_clock::now()<deadline) {
        remote.update(world_,library_,readers,device,pass,camera,viewport);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(tag->state,cgl::component::AppearanceState::Failed);EXPECT_FALSE(tag->presented);EXPECT_EQ(remote.readyVisible,0);
    const auto failed=library_.failedLoads();EXPECT_EQ(failed,1);
    for(unsigned i=0;i<8;++i) remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(library_.failedLoads(),failed); // A failed appearance does not restart every frame.
    remote.apply(world_,{3,{{1,{10,10},2}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(tag->state,cgl::component::AppearanceState::Ready);EXPECT_TRUE(tag->presented);
    EXPECT_EQ(remote.readyVisible,1); // Matching requested appearance is ready even if it shares the placeholder pointer.
    remote.apply(world_,{4,{{1,{1000,1000},2}}},placeholder);
    remote.update(world_,library_,readers,device,pass,camera,viewport);
    EXPECT_EQ(tag->state,cgl::component::AppearanceState::Ready);EXPECT_FALSE(tag->presented);
    remote.clear(world_);library_.cancelPending();EXPECT_FALSE(library_.streamingPending());
}

TEST_F(DesktopSceneTest, LateMapDecodeFailurePublishesNoPartialCacheAndCanReload) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto path=std::filesystem::path(world_.getSingleton<cgl::component::EngineState>()->settings->general.EngineRootPath)/"map/0/100.dat";
    constexpr auto version=cgl::CrossGateVersion::CG_VERSION_Classic;
    uint32_t failId=0;
    {
        std::lock_guard lock(*readers.queryMutex);
        cgl::MapLayout layout;ASSERT_EQ(cgl::IMapLayoutReader::create()->read(path,&layout),cgl::Results::Success);
        cgl::MapBuilder prepared(std::move(layout),*readers.pGfxInfoReader[version],*readers.pPaletteReader);
        ASSERT_GT(prepared.pageCount(),1);
        for(const auto& [id,image]:prepared.data().images) if(image.atlasIndex==1) {failId=id;break;}
    }
    ASSERT_NE(failId,0);
    auto proxy=std::make_unique<MapDecodeFailure>(std::move(readers.pGfxDataReader[version]),failId);
    auto* injected=proxy.get();readers.pGfxDataReader[version]=std::move(proxy);
    cgl::MapLibrary cache;
    EXPECT_THROW(cache.acquire(path,readers,device,pass),std::runtime_error);
    EXPECT_GT(injected->successful,0);EXPECT_EQ(injected->failures,1);
    EXPECT_EQ(cache.usage().maps,0);EXPECT_EQ(cache.usage().pages,0);EXPECT_EQ(cache.usage().rgbaBytes,0);
    EXPECT_EQ(cache.usage().retiredMaps,0);
    auto original=std::move(injected->source);readers.pGfxDataReader[version]=std::move(original);
    auto map=cache.acquire(path,readers,device,pass);ASSERT_NE(map,nullptr);EXPECT_GT(map->renderers.size(),1);
    std::weak_ptr<const cgl::MapResources> weak=map;
    ASSERT_EQ(device->waitIdle(),cgl::Results::Success);map.reset();cache.clear();EXPECT_TRUE(weak.expired());
    EXPECT_EQ(static_cast<cgl::graphics::vulkan::Device*>(device)->validationErrorCount(),0);
}

TEST_F(DesktopSceneTest, ProgressiveMapCancelsReaderWaitAndSubmittedUploadThenReusesSlot) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto root=std::filesystem::path(world_.getSingleton<cgl::component::EngineState>()->settings->general.EngineRootPath);
    cgl::MapLibrary cache;
    for(unsigned i=0;i<3;++i) {
        std::unique_lock held(*readers.queryMutex);
        EXPECT_EQ(cache.acquireProgressive(root/"map/0/100.dat",readers,device,pass),nullptr);
        EXPECT_TRUE(cache.progress().cpuPending);EXPECT_EQ(cache.progress().workingBytes,0);
        EXPECT_THROW(cache.acquireProgressive(root/"map/0/2100.dat",readers,device,pass),std::logic_error);
        cache.cancelPending(); // Must not wait for the held reader lock.
        EXPECT_FALSE(cache.progress().loading);EXPECT_EQ(cache.usage().pendingBytes,0);
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(!cache.progress().uploadPending && std::chrono::steady_clock::now()<deadline) {
        cache.acquireProgressive(root/"map/0/100.dat",readers,device,pass);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(cache.progress().uploadPending);EXPECT_LE(cache.progress().workingBytes,32ull*1024*1024);
    EXPECT_EQ(cache.usage().maps,0);cache.cancelPending();
    EXPECT_FALSE(cache.progress().loading);EXPECT_EQ(cache.usage().pendingBytes,0);EXPECT_EQ(cache.usage().maps,0);
    std::shared_ptr<const cgl::MapResources> map;
    bool sawCpu=false,sawUpload=false,sawCompletedPage=false;
    while(!map && std::chrono::steady_clock::now()<deadline) {
        map=cache.acquireProgressive(root/"map/0/100.dat",readers,device,pass);
        const auto p=cache.progress();sawCpu|=p.cpuPending;sawUpload|=p.uploadPending;sawCompletedPage|=p.completed>0;
        EXPECT_LE(p.workingBytes,32ull*1024*1024);EXPECT_LE(p.pendingBytes,256ull*1024*1024);
        if(!map) EXPECT_EQ(cache.usage().maps,0);
        draw_.update(&world_);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(map,nullptr);EXPECT_TRUE(sawCpu);EXPECT_TRUE(sawUpload);EXPECT_TRUE(sawCompletedPage);
    EXPECT_EQ(map->layout.width,840);EXPECT_EQ(map->layout.height,610);EXPECT_EQ(map->renderers.size(),2);
    EXPECT_EQ(map->rgbaBytes,40330752);EXPECT_EQ(cache.usage().pendingBytes,0);EXPECT_FALSE(cache.progress().loading);
    EXPECT_EQ(cache.acquireProgressive(root/"map/0/100.dat",readers,device,pass),map);
    ASSERT_EQ(device->waitIdle(),cgl::Results::Success);map.reset();cache.clear();EXPECT_EQ(cache.usage().maps,0);
}

TEST_F(DesktopSceneTest, ProgressiveMapFailureClearsPartialPagesAndSceneExitCancelsLoading) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    const auto root=std::filesystem::path(world_.getSingleton<cgl::component::EngineState>()->settings->general.EngineRootPath);
    constexpr auto version=cgl::CrossGateVersion::CG_VERSION_Classic;
    uint32_t failId=0;
    {
        std::lock_guard lock(*readers.queryMutex);cgl::MapLayout layout;
        ASSERT_EQ(cgl::IMapLayoutReader::create()->read(root/"map/0/100.dat",&layout),cgl::Results::Success);
        cgl::MapBuilder plan(std::move(layout),*readers.pGfxInfoReader[version],*readers.pPaletteReader);
        for(const auto& [id,image]:plan.data().images) if(image.atlasIndex==1) {failId=id;break;}
    }
    ASSERT_NE(failId,0);
    auto proxy=std::make_unique<MapDecodeFailure>(std::move(readers.pGfxDataReader[version]),failId);
    auto* injected=proxy.get();readers.pGfxDataReader[version]=std::move(proxy);
    cgl::MapLibrary cache;bool failed=false,sawPage=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(!failed && std::chrono::steady_clock::now()<deadline) {
        try {EXPECT_EQ(cache.acquireProgressive(root/"map/0/100.dat",readers,device,pass),nullptr);}
        catch(const std::runtime_error&) {failed=true;}
        sawPage|=cache.progress().completed>0;std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    cache.cancelPending();
    EXPECT_TRUE(failed);EXPECT_TRUE(sawPage);EXPECT_EQ(injected->failures,1);
    auto original=std::move(injected->source);readers.pGfxDataReader[version]=std::move(original);
    EXPECT_FALSE(cache.progress().loading);EXPECT_EQ(cache.usage().pendingBytes,0);EXPECT_EQ(cache.usage().maps,0);
    // Leave a real map scene while its background preparation is in flight.
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;settings.map.FilePath="map/0/100.dat";
    auto* scene=world_.getSingleton<cgl::component::SceneState>();scene->nextScene=cgl::SceneTypes::MapScene;
    manager_.update(&world_);EXPECT_TRUE(maps_.progress().loading);
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_FALSE(maps_.progress().loading);EXPECT_EQ(maps_.usage().pendingBytes,0);
    EXPECT_EQ(world_.getSingleton<cgl::component::MapCamera>(),nullptr);
}

TEST_F(DesktopSceneTest, ResourceLifecycleReturnsToQuiescentBaseline) {
    ASSERT_TRUE(world_.getSingleton<cgl::component::EngineState>()->settings->display.Validation)
        << "Lifecycle soak requires Display.Validation=true";
    const char* configured=std::getenv("CGL_SOAK_CYCLES");
    unsigned cycles=4;
    if(configured) {
        const std::string_view value(configured);
        ASSERT_FALSE(value.empty());ASSERT_LE(value.size(),4);
        cycles=0;
        for(char c:value) {ASSERT_GE(c,'0');ASSERT_LE(c,'9');cycles=cycles*10+unsigned(c-'0');}
        ASSERT_GE(cycles,1);ASSERT_LE(cycles,1000);
    }
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.animation.SerialNum=100000;
    settings.serverdemo.Enabled=true;settings.serverdemo.VisibleActors=8;
    settings.serverdemo.PrefetchActors=8;settings.serverdemo.CatalogLimit=32;
    settings.serverdemo.StressFrames=0;
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    const auto* originalReader=readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get();
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    auto* sync=world_.getSingleton<cgl::component::PrimaryRenderSyncObjects>();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    ASSERT_EQ(device->waitIdle(),cgl::Results::Success);
    library_.clear();maps_.clear();
    for(unsigned cycle=0;cycle<cycles;++cycle) {
        SCOPED_TRACE(cycle);
        settings.map.FilePath=cycle%2?"map/0/2100.dat":"map/0/100.dat";
        world_.getSingleton<cgl::component::SceneState>()->nextScene=cgl::SceneTypes::MapScene;
        ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));
        std::weak_ptr<const cgl::MapResources> map=maps_.acquire(
            std::filesystem::path(settings.general.EngineRootPath)/settings.map.FilePath,readers,device,pass);
        std::vector<std::weak_ptr<const cgl::AnimationClip>> clips;
        for(unsigned frame=0;frame<16;++frame) {
            auto* input=world_.getSingleton<cgl::component::KeyboardInputState>();
            input->moveX=frame<8?1:-1;input->newBatchRequested=frame%4==0;
            world_.getSingleton<cgl::component::FrameTime>()->deltaSeconds=.02;
            library_.maintainStreaming(sync->progress.submitted,sync->progress.completed);
            manager_.update(&world_);draw_.update(&world_);
            ASSERT_NE(world_.getSingleton<cgl::component::SceneState>()->state,cgl::StateTypes::ERROR);
            ASSERT_NE(world_.getSingleton<cgl::component::FrameRenderState>()->state,cgl::StateTypes::ERROR);
        }
        size_t activeRemote=0;
        world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++activeRemote;});
        ASSERT_EQ(activeRemote,16);
        ASSERT_GE(world_.getSingleton<cgl::component::StreamingDiagnostics>()->replacements,4);
        world_.forEach<cgl::component::Sprite>([&](auto,auto* sprite){clips.push_back(sprite->clip);});
        ASSERT_FALSE(clips.empty());
        cgl::SceneManagerDestroySystem{}.update(&world_);
        library_.retireUnused(0,sync->progress.submitted);maps_.retireUnused(0,sync->progress.submitted,0);
        EXPECT_FALSE(map.expired()); // Retired ownership survives until GPU completion.
        ASSERT_EQ(device->waitIdle(),cgl::Results::Success);
        sync->progress.didComplete(sync->progress.submitted);
        library_.collectRetired(sync->progress.completed);maps_.collectRetired(sync->progress.completed);
        EXPECT_TRUE(map.expired());for(const auto& clip:clips) EXPECT_TRUE(clip.expired());
        // Deterministic CPU metadata cancellation while reader access is unavailable.
        const cgl::AnimationRequest request{100000,0,0};
        {
            std::unique_lock held(*readers.queryMutex);
            ASSERT_EQ(library_.acquireProgressive(request,readers,device,pass),nullptr);
            ASSERT_TRUE(library_.cpuPending());EXPECT_TRUE(library_.discardUnsubmitted());
        }
        library_.cancelPending();
        // Cancel after submission too: cleanup must wait for that upload's chunk.
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!library_.uploadPending() && std::chrono::steady_clock::now()<deadline) {
            library_.acquireProgressive(request,readers,device,pass);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_TRUE(library_.uploadPending());library_.cancelPending();
        const auto a=library_.usage();const auto m=maps_.usage();
        ASSERT_EQ(a.clips,0);ASSERT_EQ(a.rgbaBytes+a.retiredBytes+a.pendingBytes,0);
        ASSERT_EQ(a.retiredClips,0);ASSERT_FALSE(library_.streamingPending());ASSERT_FALSE(library_.waitingForBudget());
        ASSERT_EQ(m.maps+m.outlines+m.pages+m.retiredMaps+m.retiredOutlines,0);
        ASSERT_EQ(m.rgbaBytes+m.outlineBytes+m.retiredMapBytes+m.retiredOutlineBytes,0);
        ASSERT_EQ(m.pendingBytes+m.workingBytes,0);ASSERT_FALSE(m.loading);ASSERT_FALSE(m.cpuPending);ASSERT_FALSE(m.uploadPending);
        size_t sprites=0,remote=0;
        world_.forEach<cgl::component::Sprite>([&](auto,auto*){++sprites;});
        world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++remote;});
        ASSERT_EQ(sprites,0);ASSERT_EQ(remote,0);
        ASSERT_EQ(world_.getSingleton<cgl::component::MapCamera>(),nullptr);
        ASSERT_EQ(world_.getSingleton<cgl::component::SceneLoadProgress>(),nullptr);
        ASSERT_EQ(world_.getSingleton<cgl::component::MapHover>(),nullptr);
        ASSERT_EQ(world_.getSingleton<cgl::component::StreamingDiagnostics>(),nullptr);
        ASSERT_EQ(readers.pGfxDataReader[cgl::CrossGateVersion::CG_VERSION_Classic].get(),originalReader);
        ASSERT_EQ(world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get(),pass);
        ASSERT_EQ(static_cast<cgl::graphics::vulkan::Device*>(device)->validationErrorCount(),0);
        // The runner samples RSS at this quiescent point before allowing a reload.
        std::cout<<"CGL_SOAK_CHECKPOINT "<<cycle+1<<std::endl;
        if(std::getenv("CGL_SOAK_HANDSHAKE")) {std::string ack;ASSERT_TRUE(bool(std::getline(std::cin,ack)));ASSERT_EQ(ack,"continue");}
    }
}

TEST_F(DesktopSceneTest, CatalogEntryIsIncrementalAndRestartReleasesPartialWorld) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    settings.serverdemo.Enabled=true;settings.serverdemo.VisibleActors=8;
    settings.serverdemo.PrefetchActors=8;settings.serverdemo.CatalogLimit=2048;
    world_.addSingleton<cgl::component::RuntimeTelemetry>();
    auto* telemetry=world_.getSingleton<cgl::component::RuntimeTelemetry>();
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    scene->nextScene=cgl::SceneTypes::MapScene;
    ASSERT_TRUE(waitMapPrepared());
    const auto catalogIndex=size_t(cgl::EntryStage::Catalog);
    ASSERT_TRUE(waitWithoutDrawing([&]{return telemetry->entry.stages[catalogIndex].calls>0;}));
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();
    ASSERT_NE(progress,nullptr);EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->completed,16);
    ASSERT_EQ(telemetry->entry.stages[catalogIndex].calls,1);
    auto remoteCount=[&]{size_t n=0;world_.forEach<cgl::component::RemoteCharacter>([&](auto,auto*){++n;});return n;};
    EXPECT_EQ(remoteCount(),0);EXPECT_EQ(world_.getSingleton<cgl::component::StreamingDiagnostics>(),nullptr);
    cgl::Entity player=0;std::weak_ptr<const cgl::AnimationClip> clip;
    world_.forEach<cgl::component::CharacterMovement,cgl::component::Sprite>([&](auto id,auto*,auto* sprite){player=id;clip=sprite->clip;});
    ASSERT_NE(player,0);ASSERT_FALSE(clip.expired());
    const auto center=world_.getSingleton<cgl::component::MapCamera>()->center;
    world_.getSingleton<cgl::component::KeyboardInputState>()->moveY=-1;
    for(int i=0;i<3;++i) {manager_.update(&world_);draw_.update(&world_);EXPECT_FALSE(progress->ready);EXPECT_EQ(remoteCount(),0);}
    EXPECT_DOUBLE_EQ(world_.getSingleton<cgl::component::MapCamera>()->center.x,center.x);
    EXPECT_DOUBLE_EQ(world_.getSingleton<cgl::component::MapCamera>()->center.y,center.y);
    world_.getSingleton<cgl::component::KeyboardInputState>()->moveY=0;
    world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(world_.getComponent<cgl::component::Sprite>(player),nullptr);
    EXPECT_EQ(world_.getSingleton<cgl::component::SceneLoadProgress>(),nullptr);EXPECT_EQ(remoteCount(),0);
    library_.clear();EXPECT_TRUE(clip.expired());EXPECT_FALSE(library_.cpuPending());EXPECT_EQ(library_.pendingBytes(),0);
    scene->nextScene=cgl::SceneTypes::MapScene;
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::MapScene));EXPECT_EQ(remoteCount(),16);
    EXPECT_GT(telemetry->entry.stages[catalogIndex].calls,128);
    EXPECT_EQ(telemetry->entry.stages[size_t(cgl::EntryStage::RemoteWorld)].calls,1);
}

TEST_F(DesktopSceneTest, ColdMapCharacterEntryCancelsCpuAndSubmittedUploadThenReloads) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& settings=*world_.getSingleton<cgl::component::EngineState>()->settings;
    settings.map.FilePath="map/0/100.dat";settings.animation.SerialNum=100000;
    auto* scene=world_.getSingleton<cgl::component::SceneState>();
    scene->nextScene=cgl::SceneTypes::MapScene;ASSERT_TRUE(waitMapPrepared());
    auto* progress=world_.getSingleton<cgl::component::SceneLoadProgress>();
    ASSERT_NE(progress,nullptr);EXPECT_EQ(progress->completed,0);EXPECT_FALSE(progress->ready);
    EXPECT_TRUE(library_.cpuPending());EXPECT_FALSE(library_.uploadPending());
    auto spriteCount=[&]{size_t count=0;world_.forEach<cgl::component::Sprite>([&](auto,auto*){++count;});return count;};
    EXPECT_EQ(spriteCount(),0);
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_FALSE(library_.cpuPending());EXPECT_FALSE(library_.uploadPending());EXPECT_EQ(library_.pendingBytes(),0);
    EXPECT_EQ(world_.getSingleton<cgl::component::SceneLoadProgress>(),nullptr);
    scene->nextScene=cgl::SceneTypes::MapScene;ASSERT_TRUE(waitMapPrepared());
    ASSERT_TRUE(waitWithoutDrawing([&]{return library_.uploadPending();}));
    progress=world_.getSingleton<cgl::component::SceneLoadProgress>();
    EXPECT_FALSE(progress->ready);EXPECT_EQ(progress->completed,0);EXPECT_EQ(spriteCount(),0);
    EXPECT_GT(library_.pendingBytes(),0);
    cgl::SceneManagerDestroySystem{}.update(&world_);
    EXPECT_EQ(library_.pendingBytes(),0);EXPECT_FALSE(library_.streamingPending());
    library_.clear();EXPECT_EQ(library_.usage().clips,0);
    scene->nextScene=cgl::SceneTypes::MapScene;ASSERT_TRUE(waitMapPrepared());
    progress=world_.getSingleton<cgl::component::SceneLoadProgress>();
    size_t previous=progress->completed;bool published=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(!progress->ready && std::chrono::steady_clock::now()<deadline) {
        manager_.update(&world_);
        ASSERT_NE(scene->state,cgl::StateTypes::ERROR)<<scene->lastError;
        EXPECT_LE(progress->completed,previous+1); // Cold clips cannot upload all sixteen in one update.
        previous=progress->completed;
        if(!progress->ready) EXPECT_EQ(spriteCount(),0);
        else published=true;
        const auto usage=library_.usage();EXPECT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,usage.budgetBytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(progress->ready);EXPECT_TRUE(published);EXPECT_EQ(progress->completed,16);EXPECT_EQ(spriteCount(),1);
    EXPECT_FALSE(library_.streamingPending());EXPECT_EQ(library_.pendingBytes(),0);
    draw_.update(&world_);world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice->waitIdle();
    std::weak_ptr<const cgl::AnimationClip> clip=library_.findReady({100000,0,0});
    cgl::SceneManagerDestroySystem{}.update(&world_);library_.clear();EXPECT_TRUE(clip.expired());
}
TEST_F(DesktopSceneTest, ProgressiveSceneClipOver32MiBWaitsForBudgetAndUsesChunkedUpload) {
    ASSERT_TRUE(enterReadyScene(cgl::SceneTypes::AnimationScene));
    auto& readers=*world_.getSingleton<cgl::component::AssetsReaderState>();
    constexpr auto version=cgl::CrossGateVersion::CG_VERSION_Classic;
    auto* device=world_.getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
    auto* pass=world_.getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
    cgl::AnimationLibrary local;
    auto pin=local.acquire({100000,1,0},readers,device,pass);
    readers.pGfxInfoReader[version]=std::make_unique<LargeCanvasInfo>(std::move(readers.pGfxInfoReader[version]));
    struct Restore {
        cgl::AnimationLibrary& local;cgl::component::AssetsReaderState& readers;
        ~Restore() {
            local.clear();auto* proxy=static_cast<LargeCanvasInfo*>(readers.pGfxInfoReader[version].get());
            auto source=std::move(proxy->source);readers.pGfxInfoReader[version]=std::move(source);
        }
    } restore{local,readers}; // Join CPU access before restoring the reader, including assertion exits.
    const cgl::AnimationRequest request{100000,0,0};cgl::AnimationBuilder planner;
    const auto bytes=planner.requiredBytes(request,readers);
    ASSERT_GT(bytes,cgl::AnimationDecodeWorker::MaxBytes);ASSERT_LE(bytes,cgl::AnimationBuilder::MaxBytes);
    auto* pixels=readers.pGfxDataReader[version].get();pixels->resetCacheStatistics();
    local.setTextureBudget(bytes);
    EXPECT_EQ(local.acquireForSceneProgressive(request,readers,device,pass),nullptr);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(!local.waitingForBudget() && std::chrono::steady_clock::now()<deadline) {
        EXPECT_EQ(local.acquireForSceneProgressive(request,readers,device,pass),nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(local.waitingForBudget());EXPECT_EQ(local.waitingBytes(request),bytes);EXPECT_EQ(local.pendingBytes(),0);
    EXPECT_EQ(pixels->cacheStatistics().fileReadCalls,0);EXPECT_FALSE(local.streamingPending());
    std::weak_ptr<const cgl::AnimationClip> old=pin;pin.reset();
    EXPECT_GT(local.trimUnused(0),0);EXPECT_TRUE(old.expired());
    size_t uploadPolls=0;std::shared_ptr<const cgl::AnimationClip> clip;
    while(!clip && std::chrono::steady_clock::now()<deadline) {
        if(local.uploadPending()) ++uploadPolls;
        clip=local.acquireForSceneProgressive(request,readers,device,pass);
        const auto usage=local.usage();EXPECT_LE(usage.rgbaBytes+usage.retiredBytes+usage.pendingBytes,bytes);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_NE(clip,nullptr);EXPECT_EQ(local.usage().rgbaBytes,bytes);EXPECT_EQ(local.pendingBytes(),0);
    EXPECT_GE(uploadPolls,(bytes+4*1024*1024-1)/(4*1024*1024));
    EXPECT_EQ(clip->renderer->pollUpload(),cgl::graphics::SpriteUploadState::Ready);
    std::weak_ptr<const cgl::AnimationClip> weak=clip;clip.reset();local.clear();EXPECT_TRUE(weak.expired());
    // The remote streaming API must still reject the very same large clip before pixels.
    local.setTextureBudget(bytes);pixels->resetCacheStatistics();bool rejected=false;
    while(!rejected && std::chrono::steady_clock::now()<deadline) {
        try {EXPECT_EQ(local.acquireProgressive(request,readers,device,pass),nullptr);}
        catch(const std::runtime_error&) {rejected=true;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(rejected);EXPECT_EQ(pixels->cacheStatistics().fileReadCalls,0);EXPECT_EQ(local.pendingBytes(),0);
}
