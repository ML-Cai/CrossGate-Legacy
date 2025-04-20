// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <stdint.h>
#include <string_view>
#include "cgl/core/version.h"

namespace cgl {

// -----------------------------------------------------------------------------
// Graphic Index/Data related section
// -----------------------------------------------------------------------------
struct GraphicsBasedIndex {
    using Key = uint64_t;

    int32_t value;
    cgl::CrossGateVersion version;

    bool operator==(const cgl::GraphicsBasedIndex& other) const {
        return value == other.value && version == other.version;
    }

    bool operator!=(const cgl::GraphicsBasedIndex& other) const {
        return value != other.value || version != other.version;
    }

    bool operator<(const cgl::GraphicsBasedIndex& other) const {
        return key() < other.key();
    }

    GraphicsBasedIndex::Key key() const noexcept {
        return static_cast<uint64_t>(version) * INT32_MAX +
               static_cast<uint64_t>(value);
    }
};

// -----------------------------------------------------------------------------
// Map Index/Data related section
// -----------------------------------------------------------------------------
struct MapBasedIndex {
    using Key = uint64_t;

    int32_t value;
    cgl::CrossGateVersion version;

    bool operator==(const cgl::MapBasedIndex& other) const {
        return value == other.value && version == other.version;
    }

    bool operator!=(const cgl::MapBasedIndex& other) const {
        return value != other.value || version != other.version;
    }

    bool operator<(const cgl::MapBasedIndex& other) const {
        return key() < other.key();
    }

    MapBasedIndex::Key key() const noexcept {
        // TODO(Miles) : The key of map index might be unique in map layout in
        // current version
        return static_cast<uint64_t>(value);
    }
};


// -----------------------------------------------------------------------------
// Graphic resource info
// -----------------------------------------------------------------------------
struct GraphicsResourceInfo {
    cgl::CrossGateVersion version;
    cgl::GraphicsBasedIndex graphicsIdx;
    cgl::MapBasedIndex mapIdx;
    uint32_t dataOffset;
    uint32_t dataSize;
    int32_t offsetX;
    int32_t offsetY;
    int32_t width;
    int32_t height;
};



}   // namespace cgl
