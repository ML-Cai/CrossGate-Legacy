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

class IDevice;

class ISemaphore {
 public:
    using Ptr = std::unique_ptr<cgl::ISemaphore>;

    static cgl::ISemaphore::Ptr create(cgl::IDevice* pDevice);

    explicit ISemaphore() = default;

    virtual ~ISemaphore() = default;
};

}   // namespace cgl
