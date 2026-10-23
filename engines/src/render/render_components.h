// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include "render/submission_progress.h"
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
struct FrameRenderState {
    cgl::StateTypes state;
    std::string lastError;
};

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
    cgl::graphics::IFence::Ptr pFence;
    uint64_t submissionSerial = 0;
};

// -----------------------------------------------------------------------------
struct PrimaryRenderSyncObjects {
    std::vector<RenderSyncObjects> objs;
    // Presentation can outlive a frame fence. Reuse after reacquiring the image.
    std::vector<cgl::graphics::ISemaphore::Ptr> presentSemaphores;
    cgl::SubmissionProgress progress;
};


}   // namespace component
}   // namespace cgl
