#include "scene/scene_components.h"
#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "map/map_render_data.h"
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "engine/error_system.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include <stdexcept>

namespace {
class MapScene final : public cgl::IScene {
public:
    MapScene(cgl::MapLibrary& maps, cgl::AnimationLibrary& animations)
        : IScene(cgl::SceneTypes::MapScene), maps_(maps), animations_(animations) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        try {
            const auto& settings = *world->getSingleton<cgl::component::EngineState>()->settings;
            const auto scale = settings.display.ViewportScale;
            if (!std::isfinite(scale) || scale > 16 || scale < 0.125)
                throw std::runtime_error("Map ViewportScale must be between 0.125 and 16");
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
            auto& readers = *world->getSingleton<cgl::component::AssetsReaderState>();
            map_ = maps_.acquire(std::filesystem::path(settings.general.EngineRootPath) / settings.map.FilePath,
                readers, render.pDevice.get(), pass);
            const cgl::MapPoint center{(map_->layout.width - 1) * 0.5, (map_->layout.height - 1) * 0.5};
            world->addSingleton<cgl::component::MapCamera>(center, scale);
            auto clip = animations_.acquire({settings.animation.SerialNum, settings.animation.Direction, settings.animation.Motion},
                readers, render.pDevice.get(), pass);
            character_ = world->createEntity();
            world->addComponent<cgl::component::MapPosition>(character_, center);
            world->addComponent<cgl::component::Sprite>(character_, std::move(clip));
            world->addComponent<cgl::component::AnimationPlayer>(character_);
            cgl::UpdateAnimations(*world, 0);
            LOGI("Map scene entered");
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map scene failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        if (character_) world->destroyEntity(character_);
        character_ = 0;
        world->removeSingleton<cgl::component::MapCamera>();
        map_.reset(); // Engine caches retain resources for pending submissions.
        drawItems_.clear();
        LOGI("Map scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        const auto seconds = world->getSingleton<cgl::component::FrameTime>()->deltaSeconds;
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        cgl::PanMapCamera(*world->getSingleton<cgl::component::MapCamera>(), input->panX, input->panY,
            seconds, map_->layout.width, map_->layout.height);
        auto& position = world->getComponent<cgl::component::MapPosition>(character_)->cell;
        cgl::MoveMapPoint(position, input->moveX, input->moveY, seconds, 100, map_->layout.width, map_->layout.height);
        cgl::UpdateAnimations(*world, seconds);
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::MapScene;
        }
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        try {
            const auto& camera = *world->getSingleton<cgl::component::MapCamera>();
            drawItems_.clear();
            const auto bounds = cgl::VisibleMapBounds(camera, viewport, map_->layout.width, map_->layout.height, map_->reachX, map_->reachY);
            for (int y = bounds.top; y <= bounds.bottom; ++y) for (int x = bounds.left; x <= bounds.right; ++x) {
                const auto& cell = *map_->layout.cell(x, y);
                if (const auto found = map_->images.find(cell.groundId); found != map_->images.end()) {
                    const auto& image = found->second;
                    record({{double(x), double(y)}, map_->renderer.get(), image.source, 0,
                        image.offsetX, image.offsetY, 0}, camera, command, viewport);
                }
                if (cgl::IsClassicMapImage(cell.objectId, true)) {
                    if (const auto found = map_->images.find(cell.objectId); found != map_->images.end()) {
                        const auto& image = found->second;
                        drawItems_.push_back({{double(x), double(y)}, map_->renderer.get(), image.source, 0,
                            image.offsetX, image.offsetY, uint64_t(y) * map_->layout.width + x});
                    }
                }
            }
            world->forEach<cgl::component::MapPosition, cgl::component::Sprite, cgl::component::AnimationPlayer>(
                [&](cgl::Entity entity, auto* position, auto* sprite, auto* player) {
                    const auto& clip = sprite->clip;
                    if (clip) drawItems_.push_back({position->cell, clip->renderer.get(), {0, 0, clip->width, clip->height},
                        player->frame, clip->offsetX, clip->offsetY, (1ull << 32) | entity});
                });
            std::sort(drawItems_.begin(), drawItems_.end(), cgl::MapDrawLess);
            for (const auto& item : drawItems_) record(item, camera, command, viewport);
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map drawing failed: ", error.what());
        }
    }
private:
    static void record(const cgl::MapDrawItem& item, const cgl::component::MapCamera& camera,
                        cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) {
        auto position = cgl::MapToScreen(item.cell, camera, viewport);
        position.x += item.offsetX * camera.scale;
        position.y += item.offsetY * camera.scale;
        if (item.renderer && cgl::MapImageVisible(position, item.source.width, item.source.height, camera.scale, viewport))
            item.renderer->drawRegion(command, viewport, item.frame, item.source,
                float(position.x), float(position.y), float(camera.scale));
    }
    cgl::MapLibrary& maps_;
    cgl::AnimationLibrary& animations_;
    std::shared_ptr<const cgl::MapResources> map_;
    cgl::Entity character_ = 0;
    std::vector<cgl::MapDrawItem> drawItems_;
};
}
cgl::IScene::Ptr cgl::CreateMapScene(MapLibrary& maps, AnimationLibrary& animations) {
    return std::make_unique<MapScene>(maps, animations);
}
