#pragma once

#include <cstdint>
#include <memory>
#include <filesystem>
#include <stdexcept>
#include <pybind11/numpy.h>
#include "cgl/assets/map_layout_reader.h"

namespace cgl::py {

// Tool-facing owned values. No Session, reader, ECS or GPU pointer is retained.
class MapSnapshot {
public:
    static std::unique_ptr<MapSnapshot> load(const std::filesystem::path& path) {
        auto result = std::make_unique<MapSnapshot>();
        const auto status = IMapLayoutReader::create()->read(path, &result->layout_);
        if (status == Results::OutOfMemory) throw std::bad_alloc();
        if (status != Results::Success)
            throw std::invalid_argument("Cannot read MAP file (missing, malformed or exceeds 4194304 cells): " + path.string());
        return result;
    }

    uint32_t width() const noexcept { return layout_.width; }
    uint32_t height() const noexcept { return layout_.height; }

    pybind11::dict cell(int64_t x, int64_t y) const {
        checkRegion(x, y, 1, 1);
        const auto& value = *layout_.cell(static_cast<int32_t>(x), static_cast<int32_t>(y));
        pybind11::dict result;
        result["ground_id"] = value.groundId;
        result["object_id"] = value.objectId;
        result["flags"] = value.flags;
        return result;
    }

    // A bounded, independently owned [height,width,3] uint16 copy, not a view.
    pybind11::array_t<uint16_t> region(int64_t x, int64_t y, int64_t width, int64_t height) const {
        checkRegion(x, y, width, height);
        pybind11::array_t<uint16_t> result({pybind11::ssize_t(height), pybind11::ssize_t(width), pybind11::ssize_t(3)});
        auto* output = result.mutable_data();
        for (int64_t row = 0; row < height; ++row) {
            for (int64_t column = 0; column < width; ++column) {
                const auto& value = *layout_.cell(static_cast<int32_t>(x + column), static_cast<int32_t>(y + row));
                *output++ = value.groundId;
                *output++ = value.objectId;
                *output++ = value.flags;
            }
        }
        return result;
    }

private:
    MapLayout layout_;
    void checkRegion(int64_t x, int64_t y, int64_t width, int64_t height) const {
        if (x < 0 || y < 0 || x >= layout_.width || y >= layout_.height ||
            width <= 0 || height <= 0 || width > int64_t(layout_.width) - x ||
            height > int64_t(layout_.height) - y)
            throw pybind11::index_error("MAP region must be positive and inside the map");
        if (uint64_t(width) * uint64_t(height) > 65536)
            throw std::invalid_argument("MAP region exceeds 65536 cells; request smaller regions");
    }
};

} // namespace cgl::py
