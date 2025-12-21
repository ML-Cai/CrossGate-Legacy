// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>

namespace cgl {
namespace graphics {

class IDevice;
class IRenderPass;

class IFramebuffer {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::IFramebuffer>;

    static cgl::graphics::IFramebuffer::Ptr create(
        cgl::graphics::IDevice*     pDevice,
        cgl::graphics::IRenderPass* pRenderpass);

    explicit IFramebuffer() = default;

    virtual ~IFramebuffer() = default;
};

}   // namespace graphics
}   // namespace cgl
