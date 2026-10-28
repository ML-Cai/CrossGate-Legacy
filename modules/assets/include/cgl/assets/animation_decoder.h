#pragma once

#include <compare>
#include <atomic>
#include <optional>
#include <stdexcept>
#include "cgl/common/assets.h"

namespace cgl {
class IGraphicsInfoReader;
class IGraphicsDataReader;
class IPaletteReader;
class IAnimeInfoReader;
class IAnimeDataReader;
struct AnimationRequest {
    uint32_t serial = 101406, direction = 0, motion = 5;
    CrossGateVersion version = CrossGateVersion::CG_VERSION_Classic;
    auto operator<=>(const AnimationRequest&) const = default;
};
// Metadata admission failure, before palette/pixel reads. The caller decides
// whether this exceeds the permanent clip limit or only currently free budget.
class AnimationBudgetExceeded : public std::runtime_error {
public:
    explicit AnimationBudgetExceeded(uint64_t bytes)
        : std::runtime_error("Animation preview exceeds remaining texture memory budget"),requiredBytes(bytes) {}
    const uint64_t requiredBytes;
};
struct AnimationPixels {
    uint32_t width = 0, height = 0, frameCount = 0, durationMs = 0;
    int32_t offsetX = 0, offsetY = 0;
    std::vector<uint8_t> rgba;
};
// Non-owning readers for exactly request.version. Caller serializes all access and
// keeps readers alive for the call; clear decoder before destroying/replacing them.
struct AnimationReaders {
    IAnimeInfoReader* animeInfo=nullptr;
    IAnimeDataReader* animeData=nullptr;
    IGraphicsInfoReader* graphicsInfo=nullptr;
    IGraphicsDataReader* graphicsData=nullptr;
    IPaletteReader* palette=nullptr;
};
// CPU-only, serialized access. Retains at most one character description.
// Clear when reloading resources in place; reader identity alone cannot detect it.
class AnimationDecoder {
public:
    static constexpr uint64_t MaxBytes=256ull*1024*1024;
    // Metadata only: validates the canvas and returns exact RGBA payload.
    uint64_t requiredBytes(const AnimationRequest& request,const AnimationReaders& readers,const std::atomic_bool* cancelled=nullptr);
    AnimationPixels build(const AnimationRequest& request, const AnimationReaders& readers,
                          uint64_t availableBytes = 256ull * 1024 * 1024, const std::atomic_bool* cancelled = nullptr);
    void clear() noexcept {
        description_.reset();
        infoReader_ = nullptr;
        dataReader_ = nullptr;
    }
private:
    struct Plan {AnimationPixels canvas;std::vector<GraphicsResourceInfo> frames;uint64_t bytes;};
    Plan prepare(const AnimationRequest&,const AnimationReaders&,const std::atomic_bool*);
    std::optional<AnimeResourceData> description_;
    const IAnimeInfoReader* infoReader_ = nullptr;
    const IAnimeDataReader* dataReader_ = nullptr;
};
}
