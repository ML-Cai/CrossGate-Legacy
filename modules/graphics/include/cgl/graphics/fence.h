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

class IFence {
 public:
    using Ptr = std::unique_ptr<cgl::graphics::IFence>;

    static cgl::graphics::IFence::Ptr create(cgl::graphics::IDevice* pDevice);

    explicit IFence() = default;

    virtual ~IFence() = default;

    virtual bool wait() = 0;

    virtual bool reset() = 0;

};

}   // namespace graphics
}   // namespace cgl
