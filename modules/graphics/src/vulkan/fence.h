// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include "cgl/graphics/fence.h"

namespace cgl {
namespace graphics {
namespace vulkan {

class Fence : public cgl::graphics::IFence {
 public:
    explicit Fence(VkDevice device);

    virtual ~Fence();

    bool prepare();

    void destroy();

    VkFence fence() const noexcept { return fence_;}

    bool wait() override;

    bool reset() override;

 private:
    VkDevice device_;
    VkFence fence_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
