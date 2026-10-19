#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
#include "cgl/common/results.h"

namespace cgl {

// Raw IDs and flags are preserved; version-specific interpretation belongs to
// the consumer. Cells are row-major, with x increasing within each row.
struct MapCell {
    uint16_t groundId = 0;
    uint16_t objectId = 0;
    uint16_t flags = 0;
};

struct MapLayout {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<MapCell> cells;

    const MapCell* cell(int32_t x, int32_t y) const noexcept {
        if (x < 0 || y < 0 || static_cast<uint32_t>(x) >= width ||
            static_cast<uint32_t>(y) >= height) return nullptr;
        const auto index = static_cast<size_t>(y) * width + x;
        return index < cells.size() ? &cells[index] : nullptr;
    }
};

class IMapLayoutReader {
public:
    using Ptr = std::unique_ptr<IMapLayoutReader>;
    static Ptr create();
    virtual ~IMapLayoutReader() = default;

    // Reads a 20-byte MAP header and three little-endian uint16 planes.
    // Rejects trailing bytes and maps larger than 4,194,304 cells.
    // On failure, the destination remains unchanged.
    virtual Results read(const std::filesystem::path& path, MapLayout* layout) = 0;
};

} // namespace cgl
