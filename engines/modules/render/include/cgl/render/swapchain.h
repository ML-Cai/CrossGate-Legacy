// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include "cgl/common/math.h"
#include <memory>

namespace cgl {

class IDevice;
class IWindow;
class ISemaphore;

class ISwapchain {
 public:
    using Ptr = std::unique_ptr<cgl::ISwapchain>;

    static cgl::ISwapchain::Ptr create(cgl::IDevice* pDevice,
                                       void* pWindowNativeHandle);

    explicit ISwapchain() = default;

    virtual ~ISwapchain() = default;

    virtual uint32_t imageCount() const = 0;

    virtual cgl::Size2u extent() const = 0;

    virtual bool acquireNextImageIndex(
        cgl::ISemaphore* pSemaphore,
        uint32_t*        pImageIndex = nullptr) = 0;

    virtual uint32_t acquireCurrentImageIndex() const = 0;
};

}   // namespace cgl
