// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "cgl/common/states.h"
#include "cgl/graphics/device.h"
#include "cgl/graphics/swapchain.h"
#include "cgl/graphics/semaphore.h"
#include "cgl/graphics/render_pass.h"
#include "cgl/graphics/command_buffer_list.h"
#include "cgl/graphics/fence.h"

namespace cgl {
namespace component {

// -----------------------------------------------------------------------------
struct RenderDeviceState {
    cgl::StateTypes state;
    std::string lastError;

    cgl::graphics::IDevice::Ptr pDevice;          // primary device
    cgl::graphics::ISwapchain::Ptr pSwapchain;    // primary swapchain
};

// -----------------------------------------------------------------------------
struct PrimarySceneRenderFrame {
    cgl::graphics::ICommandBufferList::Ptr pCmdBufferList;
    std::unordered_map<std::string, cgl::graphics::IRenderPass::Ptr> renderPasses;
};

// -----------------------------------------------------------------------------
struct RenderSyncObjects {
    uint32_t index;
    cgl::graphics::ISemaphore::Ptr pImageAvailableSemaphore;
    cgl::graphics::ISemaphore::Ptr pRenderFinishSemaphore;
    cgl::graphics::IFence::Ptr pFence;
};

// -----------------------------------------------------------------------------
struct PrimaryRenderSyncObjects {
    std::vector<RenderSyncObjects> objs;
};


}   // namespace component
}   // namespace cgl
