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
namespace graphics {

class IDevice;
class ICommandBuffer;
class ISwapchain;
class IFramebuffer;

class IRenderPass {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::IRenderPass>;

    enum class Types {
        InitScene,
        MainScene,
    };

    static cgl::graphics::IRenderPass::Ptr create(
        cgl::graphics::IRenderPass::Types type,
        cgl::graphics::IDevice*           pDevice,
        cgl::graphics::ISwapchain*        pSwapchain);

    explicit IRenderPass() = default;

    virtual ~IRenderPass() = default;

    virtual bool begin(const cgl::Viewport&           viewport,
                       cgl::graphics::ICommandBuffer* pCmdBuffer,
                       cgl::graphics::IFramebuffer*   pFramebuffer) = 0;

    virtual cgl::graphics::IFramebuffer* acquireFramebuffer(uint32_t idx) const = 0;

    virtual bool end() = 0;
};

}   // namespace graphics
}   // namespace cgl
