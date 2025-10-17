// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>

// -----------------------------------------------------------------------------
// forward declaration
struct GLFWwindow;

namespace cgl {

class ECSCore;

}   // namespace cgl

// -----------------------------------------------------------------------------
namespace cgl {

class RenderDeviceInitSystem {
 public:
    RenderDeviceInitSystem() = default;

    ~RenderDeviceInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool initEssentialRenderObjects(cgl::ECSCore* pECS);
};

// -----------------------------------------------------------------------------
class RenderDeviceDestroySystem {
 public:
    RenderDeviceDestroySystem() = default;

    ~RenderDeviceDestroySystem() = default;

    void update(cgl::ECSCore* pECS);
};


}   // namespace cgl
