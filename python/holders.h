// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <vector>
#include <optional>
#include <stdexcept>
#include <memory>
#include "cgl/resources/resource_types.h"
#include "cgl/resources/anime_resource.h"

namespace cgl {
namespace py {

// -----------------------------------------------------------------------------
struct PaletteData256Holder {
    using Ptr = std::shared_ptr<cgl::py::PaletteData256Holder>;

    cgl::PaletteData256 data;
};


// -----------------------------------------------------------------------------
struct GraphicsResourceBundleHolder {
    using Ptr = std::shared_ptr<cgl::py::GraphicsResourceBundleHolder>;

    cgl::GraphicsResourceInfo info;
    cgl::GraphicsResourceData data;

    std::pair<int32_t, int32_t> offset () {
        return std::make_pair(info.offsetX, info.offsetY);
    }
};

// -----------------------------------------------------------------------------
struct AnimeResourceDataHolder {
    using Ptr = std::shared_ptr<cgl::py::AnimeResourceDataHolder>;

    cgl::AnimeResourceData animeData;

    std::vector<cgl::AnimeDataMapKey> keys() const {
        std::vector<cgl::AnimeDataMapKey> keys;

        keys.reserve(animeData.motionMap.size());
        for (const auto& pair : animeData.motionMap) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    std::optional<cgl::AnimeMotionDesc> acquire(
        cgl::DirectionTypes dir,
        cgl::MotionTypes    motion
    ) const {
        auto it = animeData.motionMap.find({dir, motion});
        if (it != animeData.motionMap.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

}   // namespace py
}   // namespace cgl