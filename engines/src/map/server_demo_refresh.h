#pragma once
#include "map/map_camera.h"
namespace cgl {
// Keep refresh decisions deterministic and independent of the wall clock.
inline bool ServerDemoRefreshDue(const component::MapCamera& camera,MapPoint previousCenter,
    const Viewport& viewport,double elapsedSeconds,bool requested) noexcept {
    if(!viewport.width || !viewport.height) return false;
    const auto shift=MapToScreen(previousCenter,camera,viewport);
    return requested || elapsedSeconds>=2 ||
        std::abs(shift.x-viewport.width*.5)>viewport.width*.125 ||
        std::abs(shift.y-viewport.height*.5)>viewport.height*.125;
}
}
