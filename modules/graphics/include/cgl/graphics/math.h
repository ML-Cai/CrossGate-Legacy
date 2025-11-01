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

class IFence {
 public:
    using Ptr = std::unique_ptr<cgl::IFence>;

    static cgl::IFence::Ptr create(cgl::IDevice* pDevice);

    explicit IFence() = default;

    virtual ~IFence() = default;

    virtual bool wait() = 0;

    virtual bool reset() = 0;

};

}   // namespace cgl
