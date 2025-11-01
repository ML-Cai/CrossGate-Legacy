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
#include "cgl/render/device.h"
#include "cgl/render/swapchain.h"
#include "cgl/render/semaphore.h"
#include "cgl/render/render_pass.h"
#include "cgl/render/command_buffer_list.h"
#include "cgl/render/fence.h"

namespace cgl {
namespace component {

// -----------------------------------------------------------------------------
struct RenderDeviceState {
    cgl::StateTypes state;
    std::string lastError;

    cgl::IDevice::Ptr pDevice;          // primary device
    cgl::ISwapchain::Ptr pSwapchain;    // primary swapchain
};

// -----------------------------------------------------------------------------
struct PrimarySceneRenderFrame {
    cgl::ICommandBufferList::Ptr pCmdBufferList;
    std::unordered_map<std::string, cgl::IRenderPass::Ptr> renderPasses;
};

// -----------------------------------------------------------------------------
struct RenderSyncObjects {
    uint32_t index;
    cgl::ISemaphore::Ptr pImageAvailableSemaphore;
    cgl::ISemaphore::Ptr pRenderFinishSemaphore;
    cgl::IFence::Ptr pFence;
};

// -----------------------------------------------------------------------------
struct PrimaryRenderSyncObjects {
    std::vector<RenderSyncObjects> objs;
};


}   // namespace component
}   // namespace cgl
