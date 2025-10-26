// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <stdexcept>
#include "cgl/common/matrix.h"

// -----------------------------------------------------------------------------
cgl::Mat4x4 cgl::ortho(
    float left, float right,
    float bottom, float top,
    float zNear, float zFar
) {
    cgl::Mat4x4 result;
    float width = right - left;
    float height = top - bottom;
    float depth = zFar - zNear;

    result(0, 0) = 2.0f / width;
    result(1, 1) = -2.0f / height;
    result(2, 2) = 1.0f / depth;
    result(0, 3) = -(right + left) / width;
    result(1, 3) = -(top + bottom) / height;
    result(2, 3) = -zNear / depth;

    return result;
}
