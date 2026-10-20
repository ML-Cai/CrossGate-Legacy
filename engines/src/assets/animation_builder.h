#pragma once

#include <compare>
#include <optional>
#include "cgl/common/assets.h"

namespace cgl {
namespace component { struct AssetsReaderState; }
class IAnimeInfoReader;
class IAnimeDataReader;
struct AnimationRequest {
    uint32_t serial = 101406, direction = 0, motion = 5;
    auto operator<=>(const AnimationRequest&) const = default;
};
struct AnimationPixels {
    uint32_t width = 0, height = 0, frameCount = 0, durationMs = 0;
    int32_t offsetX = 0, offsetY = 0;
    std::vector<uint8_t> rgba;
};
// CPU-only, serialized access. Retains at most one character description.
// Clear when reloading resources in place; reader identity alone cannot detect it.
class AnimationBuilder {
public:
    AnimationPixels build(const AnimationRequest& request, component::AssetsReaderState& readers,
                          uint64_t availableBytes = 256ull * 1024 * 1024);
    void clear() noexcept {
        description_.reset();
        infoReader_ = nullptr;
        dataReader_ = nullptr;
    }
private:
    std::optional<AnimeResourceData> description_;
    const IAnimeInfoReader* infoReader_ = nullptr;
    const IAnimeDataReader* dataReader_ = nullptr;
};
}
