// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <stdint.h>
#include "cgl/core/version.h"

namespace cgl {


// -----------------------------------------------------------------------------
// GraphicsResourceIndexTypes
// -----------------------------------------------------------------------------
#define GraphicsResourceIndexTypes_ENUM_LIST    \
    CGL_X(GraphicsBasedIndex)           \
    CGL_X(MapBasedIndex)                \

enum class GraphicsResourceIndexTypes : uint8_t {
#define CGL_X(name) name,
    GraphicsResourceIndexTypes_ENUM_LIST
#undef CGL_X
};

// -----------------------------------------------------------------------------
// Graphic Index/Data related section
// -----------------------------------------------------------------------------
struct GraphicsResourceIndex {
    using Key = uint64_t;

    cgl::GraphicsResourceIndexTypes type;
    cgl::CrossGateVersion version;
    int32_t value;

    bool operator==(const cgl::GraphicsResourceIndex& other) const {
        return (type == other.type) &&
               (value == other.value) &&
               (version == other.version);
    }

    bool operator!=(const cgl::GraphicsResourceIndex& other) const {
        return (type != other.type) ||
               (value != other.value) ||
               (version != other.version);
    }

    bool operator<(const cgl::GraphicsResourceIndex& other) const {
        if (this->type != other.type) {
            return false;
        }
        return key() < other.key();
    }

    GraphicsResourceIndex::Key key() const noexcept {
        return static_cast<uint64_t>(version) * INT32_MAX +
               static_cast<uint64_t>(value);
    }
};

// -----------------------------------------------------------------------------
// Graphic resource info
// -----------------------------------------------------------------------------
struct GraphicsResourceInfo {
    cgl::CrossGateVersion version;
    cgl::GraphicsResourceIndex gfxBasedsIdx;
    cgl::GraphicsResourceIndex mapBasedsIdx;
    uint32_t dataOffset;
    uint32_t dataSize;
    int32_t offsetX;
    int32_t offsetY;
    int32_t width;
    int32_t height;
};



}   // namespace cgl
