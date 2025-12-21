// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>

namespace cgl {
namespace graphics {

class IDevice;

class ISemaphore {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::ISemaphore>;

    static cgl::graphics::ISemaphore::Ptr create(
        cgl::graphics::IDevice* pDevice);

    explicit ISemaphore() = default;

    virtual ~ISemaphore() = default;
};

}   // namespace graphics
}   // namespace cgl
