// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string_view>

namespace cgl {
namespace graphics {

class IDevice;

// -----------------------------------------------------------------------------
class IBuffer {
 public:
    using Ptr = std::unique_ptr<IBuffer>;

    // BufferObjectTypes enum
    #define CGL_IBUFFER_OBJECT_TYPES_ENUM_LIST  \
        CGL_X(VertexBuffer)                     \
        CGL_X(IndexBuffer)                      \
        CGL_X(UniformBuffer)                    \
        CGL_X(Unknown)

    enum class Types : uint8_t {
    #define CGL_X(name) name,
        CGL_IBUFFER_OBJECT_TYPES_ENUM_LIST
    #undef CGL_X
    };

    IBuffer() = default;

    virtual ~IBuffer() = default;

    virtual cgl::graphics::IBuffer::Types type() const noexcept = 0;

    virtual size_t capacity() const noexcept = 0;

    virtual size_t offset() const noexcept = 0;

    virtual std::string_view name() const noexcept = 0;
};

// -----------------------------------------------------------------------------
std::string_view ToStr(const cgl::graphics::IBuffer::Types& type);

// -----------------------------------------------------------------------------
}   // namespace graphics
}   // namespace cgl
