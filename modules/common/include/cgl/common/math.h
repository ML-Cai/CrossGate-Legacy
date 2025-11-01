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

// -----------------------------------------------------------------------------
// Regular data types
// -----------------------------------------------------------------------------
template<class T>
class BaseSize_ {
 public:
    BaseSize_() {}
    BaseSize_(T w, T h) : width(w), height(h) {}
    BaseSize_(const BaseSize_& size) : width(size.width), height(size.height) {}
    ~BaseSize_() {}

    BaseSize_& operator= (const BaseSize_& size) {
        width = size.width;
        height = size.height;
        return *this;
    }

    bool operator==(const BaseSize_& other) const {
        return width == other.width && height == other.height;
    }

    bool operator!=(const BaseSize_& other) const {
        return !(*this == other);
    }

    T width;
    T height;
};

using Size2u = cgl::BaseSize_<uint32_t>;
using Size2i = cgl::BaseSize_<int32_t>;
using Size2f = cgl::BaseSize_<float>;

}   // namespace cgl
