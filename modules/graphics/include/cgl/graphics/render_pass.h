// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string_view>
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

    struct BuiltinKeys {
        static constexpr std::string_view InitScene = "InitScene";
    };


    static cgl::graphics::IRenderPass::Ptr create(
        std::string_view           renderPassKey,
        cgl::graphics::IDevice*    pDevice,
        cgl::graphics::ISwapchain* pSwapchain);

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
