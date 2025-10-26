// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <array>

namespace cgl {

// -----------------------------------------------------------------------------
class Mat4x4 {
 public:
    Mat4x4() {
        data_.fill(0.0f);
        data_[0] = 1.0f;  // M[0,0]
        data_[5] = 1.0f;  // M[1,1]
        data_[10] = 1.0f; // M[2,2]
        data_[15] = 1.0f; // M[3,3]
    }

    ~Mat4x4() = default;

    float& operator()(int row, int col) noexcept {
        return data_[col * 4 + row];
    }

    const float& operator()(int row, int col) const noexcept {
        return data_[col * 4 + row];
    }

    const float* data() const noexcept {
        return data_.data();
    }

 private:
    std::array<float, 16> data_;
};

// -----------------------------------------------------------------------------
cgl::Mat4x4 ortho(float left, float right,
                  float bottom, float top,
                  float zNear, float zFar);

// -----------------------------------------------------------------------------

}   // namespace cgl
