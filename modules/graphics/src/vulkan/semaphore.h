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
namespace vk {

class Semaphore : public cgl::ISemaphore {
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

}   // namespace vk
}   // namespace cgl
