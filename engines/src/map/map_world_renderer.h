#pragma once
#include "assets/map_library.h"
#include "map/map_hover.h"
#include "engine/ecs.h"
namespace cgl {
struct MapRenderStats { uint32_t drawnActors=0,drawCalls=0; double sortMs=0; };
// Borrows immutable resource data only for this recording call. Does not advance
// animation, move entities, update hover or acquire/decode/upload resources.
class MapWorldRenderer {
public:
    MapRenderStats draw(ECSCore& world,const MapResources& map,
        const component::MapCamera& camera,std::optional<MapCellCoord> hovered,
        const MapHoverOutline* outline,graphics::ICommandBuffer* command,const Viewport& viewport);
    void clear() {items_.clear();}
private:
    static bool record(const MapDrawItem& item,const component::MapCamera& camera,
        graphics::ICommandBuffer* command,const Viewport& viewport);
    std::vector<MapDrawItem> items_; // Scratch pointers, never resource ownership.
};
}
