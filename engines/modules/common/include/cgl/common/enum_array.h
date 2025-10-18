// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <cstdint>

namespace cgl {

template <typename Enum, typename T>
struct EnumArray {
    static constexpr size_t COUNT = static_cast<size_t>(Enum::COUNT);
    std::array<T, COUNT> data;

    constexpr T& operator[](Enum e) noexcept {
        return data[static_cast<size_t>(e)];
    }
    constexpr const T& operator[](Enum e) const noexcept {
        return data[static_cast<size_t>(e)];
    }
};

}   // namespace cgl
