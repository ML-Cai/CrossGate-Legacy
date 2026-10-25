#pragma once
#include "map/map_data.h"
#include "map/map_atlas.h"
#include "cgl/assets/graphics_info_reader.h"
#include "cgl/assets/graphics_data_reader.h"
#include "cgl/assets/palette_reader.h"
#include <utility>
#include <atomic>

namespace cgl {
struct MapPagePixels {
    uint32_t width, height;
    std::vector<uint8_t> rgba; // Owned, top-down RGBA8 with transparent borders.
};

// Transient CPU preparation. Construction plans all pages without decoding pixels.
// Each buildPage allocates only that page (<=32 MiB); discard it after upload/use.
// Reader calls must be externally serialized. No reader is retained; readers may
// be destroyed after the last buildPage. Concurrent calls are not an API guarantee.
class MapBuilder {
public:
    MapBuilder(MapLayout layout, IGraphicsInfoReader& info, IPaletteReader& palette,
        const std::atomic_bool* cancelled=nullptr);
    MapBuilder(const MapBuilder&) = delete;
    MapBuilder& operator=(const MapBuilder&) = delete;
    MapBuilder(MapBuilder&&) noexcept = default;
    MapBuilder& operator=(MapBuilder&&) noexcept = default;
    const MapData& data() const noexcept { return data_; }
    size_t pageCount() const noexcept { return pages_.size(); }
    uint64_t pageBytes(size_t index) const { const auto& p=pages_.at(index);return uint64_t(p.width)*p.height*4; }
    MapPagePixels buildPage(size_t index, IGraphicsDataReader& reader,
        uint64_t maxBytes = 32ull*1024*1024,const std::atomic_bool* cancelled=nullptr) const;
    // Consume metadata after the last page. Further page requests are invalid.
    MapData takeData() && {
        pages_.clear();infos_.clear();
        return std::exchange(data_, {});
    }
private:
    MapData data_;
    PaletteData256 palette_{};
    std::vector<MapAtlasPlan> pages_;
    std::unordered_map<uint16_t, GraphicsResourceInfo> infos_;
};
}
