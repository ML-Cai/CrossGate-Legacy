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

class IDevice;
class IRenderPass;

class IFramebuffer {
 public:
    using Ptr = std::unique_ptr<cgl::IFramebuffer>;

    static cgl::IFramebuffer::Ptr create(cgl::IDevice*     pDevice,
                                         cgl::IRenderPass* pRenderpass);

    explicit IFramebuffer() = default;

    virtual ~IFramebuffer() = default;
};

}   // namespace cgl
