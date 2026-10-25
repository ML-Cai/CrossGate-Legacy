#include "map/map_world_renderer.h"
#include "animation/animation_components.h"
#include "map/remote_character_components.h"
#include <chrono>
namespace cgl {
MapRenderStats MapWorldRenderer::draw(ECSCore& world,const MapResources& map,
    const component::MapCamera& camera,std::optional<MapCellCoord> hovered,
    const MapHoverOutline* outline,graphics::ICommandBuffer* command,const Viewport& viewport) {
    MapRenderStats stats;
    items_.clear();
    // Never retain borrowed renderer pointers across frames, including failures.
    struct Reset {std::vector<MapDrawItem>& items;~Reset(){items.clear();}} reset{items_};
    auto submit=[&](const MapDrawItem& item) {
        if(record(item,camera,command,viewport)) {
            ++stats.drawCalls;
            if(item.stableId & (1ull<<32)) ++stats.drawnActors;
        }
    };
    const auto bounds = cgl::VisibleMapBounds(camera, viewport, map.layout.width, map.layout.height, map.reachX, map.reachY);
    for (int y = bounds.top; y <= bounds.bottom; ++y) for (int x = bounds.left; x <= bounds.right; ++x) {
        const auto& cell = *map.layout.cell(x, y);
        if (const auto found = map.images.find(cell.groundId); found != map.images.end()) {
            const auto& image = found->second;
            submit({{double(x), double(y)}, map.renderers.at(image.atlasIndex).get(), image.source, 0,
                image.offsetX, image.offsetY, 0});
        }
        if (cgl::IsClassicMapImage(cell.objectId, true)) {
            if (const auto found = map.images.find(cell.objectId); found != map.images.end()) {
                const auto& image = found->second;
                items_.push_back({{double(x), double(y)}, map.renderers.at(image.atlasIndex).get(), image.source, 0,
                    image.offsetX, image.offsetY, uint64_t(y) * map.layout.width + x,
                    image.tileX, image.tileY});
            }
        }
    }
    world.forEach<cgl::component::MapPosition, cgl::component::Sprite, cgl::component::AnimationPlayer>(
        [&](cgl::Entity entity, const auto* position, const auto* sprite, const auto* player) {
            if(const auto* remote=world.getComponent<component::RemoteCharacter>(entity); remote && !remote->presented) {
                // Ground markers precede world objects, so buildings still occlude
                // them. Borrow the scene's cached outline: no per-actor GPU upload,
                // ownership, or work added to the appearance streaming budget.
                if(outline && outline->renderer) {
                    const auto center=MapToScreen(position->cell,camera,viewport);
                    const double scale=camera.scale/outline->scale;
                    const MapPoint origin{center.x-outline->width*scale*.5,center.y-outline->height*scale*.5};
                    if(MapImageVisible(origin,outline->width,outline->height,scale,viewport)) {
                        outline->renderer->draw(command,viewport,0,float(origin.x),float(origin.y),float(scale));
                        ++stats.fallbackMarkers;++stats.drawCalls;
                    }
                }
                return;
            }
            const auto& clip = sprite->clip;
            if (clip) items_.push_back({position->cell, clip->renderer.get(), {0, 0, clip->width, clip->height},
                player->frame, clip->offsetX, clip->offsetY, (1ull << 32) | entity, 0, 0});
        });
    const auto sortStart=std::chrono::steady_clock::now();
    sorter_.sort(items_);
    stats.sortMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-sortStart).count();
    for(const auto& item:items_) submit(item);
    if (hovered && outline) {
        const auto center = cgl::MapToScreen({double(hovered->x), double(hovered->y)}, camera, viewport);
        const double scale = camera.scale / outline->scale;
        outline->renderer->draw(command, viewport, 0,
            float(center.x - outline->width * scale * 0.5),
            float(center.y - outline->height * scale * 0.5), float(scale));
    }
    return stats;
}
bool MapWorldRenderer::record(const cgl::MapDrawItem& item, const cgl::component::MapCamera& camera,
                    cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) {
    auto position = cgl::MapToScreen(item.cell, camera, viewport);
    position.x += item.offsetX * camera.scale;
    position.y += item.offsetY * camera.scale;
    if (!item.renderer || !cgl::MapImageVisible(position, item.source.width, item.source.height, camera.scale, viewport)) return false;
    item.renderer->drawRegion(command, viewport, item.frame, item.source,
        float(position.x), float(position.y), float(camera.scale));
    return true;
}
}
