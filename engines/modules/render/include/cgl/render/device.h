// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include <memory>
#include "cgl/common/results.h"

namespace cgl {

class IQueue;

class IDevice {
 public:
    using Ptr = std::unique_ptr<cgl::IDevice>;

    cgl::IDevice::Ptr static create(
        void* windowNativeHandle,
        bool enableDebug);

    IDevice() = default;

    virtual ~IDevice() = default;

    virtual bool getLastError(std::string* pError = nullptr) = 0;

    virtual cgl::IQueue* presentQueue() = 0;

    virtual cgl::IQueue* graphicsQueue() = 0;

    virtual cgl::Results waitIdle() = 0;
};

}   // namespace cgl
