#pragma once
#include <cstddef>
#include <cstdint>

namespace cgl::component {
enum class AppearanceState { Queued, Loading, Deferred, Failed, Ready };

struct RemoteCharacter {
    uint64_t serverId;
    size_t appearance;
    AppearanceState state = AppearanceState::Queued;
    // A ready prefetched clip remains cache-owned until its pixels are visible.
    // Sprite may hold the legacy shared placeholder; never present it as this actor.
    bool presented = false;
};
}
