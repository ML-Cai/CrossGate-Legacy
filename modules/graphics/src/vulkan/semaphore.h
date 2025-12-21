// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <vulkan/vulkan_core.h>
#include "cgl/graphics/semaphore.h"

namespace cgl {
namespace graphics {
namespace vulkan {

class Semaphore : public cgl::graphics::ISemaphore {
 public:
    explicit Semaphore(VkDevice device);

    virtual ~Semaphore();

    bool prepare();

    void destroy();

    VkSemaphore semaphore() const noexcept { return semaphore_;}

 private:
    VkDevice device_;
    VkSemaphore semaphore_;
};

}   // namespace vulkan
}   // namespace graphics
}   // namespace cgl
