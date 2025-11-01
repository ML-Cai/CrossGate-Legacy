// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include "cgl/common/viewport.h"

namespace cgl {

class IDevice;
class ISwapchain;
class ICommandBuffer;
class IFramebuffer;

class IRenderPass {
 public:
    using Ptr = std::unique_ptr<cgl::IRenderPass>;

    enum class Types {
        InitScene,
        MainScene,
    };

    static cgl::IRenderPass::Ptr create(
        cgl::IRenderPass::Types      type,
        cgl::IDevice*                pDevice,
        cgl::ISwapchain*             pSwapchain);

    explicit IRenderPass() = default;

    virtual ~IRenderPass() = default;

    virtual bool begin(const cgl::Viewport& viewport,
                       cgl::ICommandBuffer* pCmdBuffer,
                       cgl::IFramebuffer*   pFramebuffer) = 0;

    virtual cgl::IFramebuffer* acquireFramebuffer(uint32_t idx) const = 0;

    virtual bool end() = 0;
};

}   // namespace cgl
