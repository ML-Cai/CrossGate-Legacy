
// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <cstdint>

namespace cgl {

struct Viewport {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;


    bool operator==(const cgl::Viewport& other) const {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }

    bool operator!=(const cgl::Viewport& other) const {
        return !(*this == other);
    }
};

}   // namespace cgl
