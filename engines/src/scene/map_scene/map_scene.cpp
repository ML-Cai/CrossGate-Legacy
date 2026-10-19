#include "scene/scene_components.h"
#include "assets/ground_map_library.h"
#include "assets/assets_components.h"
#include "map/map_camera.h"
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
    explicit MapScene(cgl::GroundMapLibrary& library) : IScene(cgl::SceneTypes::MapScene), library_(library) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        try {
            const auto& settings = *world->getSingleton<cgl::component::EngineState>()->settings;
            const auto scale = settings.display.ViewportScale;
            if (!std::isfinite(scale) || scale <= 0 || scale > 16 || scale < 0.125)
                throw std::runtime_error("Map ViewportScale must be between 0.125 and 16");
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
            map_ = library_.acquire(std::filesystem::path(settings.general.EngineRootPath) / settings.map.FilePath,
                *world->getSingleton<cgl::component::AssetsReaderState>(), render.pDevice.get(), pass);
            world->addSingleton<cgl::component::MapCamera>(cgl::MapPoint{
                (map_->layout.width - 1) * 0.5, (map_->layout.height - 1) * 0.5}, scale);
            LOGI("Map scene entered");
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Ground map failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        world->removeSingleton<cgl::component::MapCamera>();
        map_.reset(); // Library keeps resources alive for pending submissions.
        LOGI("Map scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        cgl::PanMapCamera(*world->getSingleton<cgl::component::MapCamera>(), input->panX, input->panY,
            world->getSingleton<cgl::component::FrameTime>()->deltaSeconds, map_->layout.width, map_->layout.height);
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::MapScene;
        }
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        if (!map_ || !map_->renderer) return;
        const auto& camera = *world->getSingleton<cgl::component::MapCamera>();
        const auto bounds = cgl::VisibleMapBounds(camera, viewport, map_->layout.width, map_->layout.height, map_->reachX, map_->reachY);
        for (int y = bounds.top; y <= bounds.bottom; ++y) for (int x = bounds.left; x <= bounds.right; ++x) {
            const auto id = map_->layout.cell(x, y)->groundId;
            const auto found = map_->tiles.find(id);
            if (found == map_->tiles.end()) continue;
            const auto& tile = found->second;
            auto position = cgl::MapToScreen({double(x), double(y)}, camera, viewport);
            position.x += tile.offsetX * camera.scale; position.y += tile.offsetY * camera.scale;
            if (position.x >= viewport.width || position.y >= viewport.height ||
                position.x + map_->frameWidth * camera.scale <= 0 ||
                position.y + map_->frameHeight * camera.scale <= 0) continue;
            map_->renderer->draw(command, viewport, tile.frame, float(position.x), float(position.y), float(camera.scale));
        }
    }
private:
    cgl::GroundMapLibrary& library_;
    std::shared_ptr<const cgl::GroundMap> map_;
};
}
cgl::IScene::Ptr cgl::CreateMapScene(GroundMapLibrary& library) { return std::make_unique<MapScene>(library); }
