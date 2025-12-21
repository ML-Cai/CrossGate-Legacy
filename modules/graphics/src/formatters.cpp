// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <sstream>
#include "cgl/graphics/buffer.h"

// -----------------------------------------------------------------------------
std::string_view cgl::graphics::ToStr(const cgl::graphics::IBuffer::Types& type) {
    switch (type) {
#define CGL_X(name) case cgl::graphics::IBuffer::Types::name: return #name;
        CGL_IBUFFER_OBJECT_TYPES_ENUM_LIST
#undef CGL_X
    default:
        return "Unknown";
    }
}
