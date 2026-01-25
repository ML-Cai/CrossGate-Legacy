// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <gtest/gtest.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "cgl/common/viewport.h"
#include "cgl/graphics/device.h"
#include "cgl/graphics/swapchain.h"
#include "cgl/graphics/queue.h"
#include "cgl/graphics/buffer.h"
#include "cgl/graphics/fence.h"
#include "cgl/graphics/semaphore.h"
#include "cgl/graphics/staging_buffer.h"
#include "cgl/graphics/command_buffer_list.h"
#include "cgl/graphics/buffer_upload_manager.h"
#include "cgl/graphics/transfer_buffer.h"
#include "cgl/graphics/buffer_allocator.h"


// -----------------------------------------------------------------------------
namespace {

constexpr static int32_t WIN_W = 256;
constexpr static int32_t WIN_H = 256;

class BasicDrawTest : public testing::Test {
 protected:
    void SetUp() override {
        EXPECT_EQ(glfwInit(), GLFW_TRUE);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIN_W, WIN_H, "test", nullptr, nullptr);
        EXPECT_NE(window_, nullptr);
    }

    GLFWwindow *window_;
};

}   // namespace


// -----------------------------------------------------------------------------
TEST_F(BasicDrawTest, BasicDrawFlow) {
    // -------------------------------------------------------------------------
    // create objects
    // -------------------------------------------------------------------------
    auto pDevice = cgl::graphics::IDevice::create(window_, true);
    EXPECT_NE(pDevice, nullptr);

    auto pSwapchain = cgl::graphics::ISwapchain::create(pDevice.get(), window_);
    EXPECT_NE(pSwapchain, nullptr);
    EXPECT_GE(pSwapchain->imageCount(), 0);
    EXPECT_EQ(pSwapchain->extent().width, WIN_W);
    EXPECT_EQ(pSwapchain->extent().height, WIN_H);

    auto pFence = cgl::graphics::IFence::create(pDevice.get());
    EXPECT_NE(pFence, nullptr);

    auto pImageAvailableSemaphore = cgl::graphics::ISemaphore::create(pDevice.get());
    auto pRenderFinishSemaphore = cgl::graphics::ISemaphore::create(pDevice.get());
    EXPECT_NE(pImageAvailableSemaphore, nullptr);
    EXPECT_NE(pRenderFinishSemaphore, nullptr);

    auto pCmdBufferList = cgl::graphics::ICommandBufferList::create(
                            pDevice.get(), pSwapchain->imageCount());
    EXPECT_NE(pCmdBufferList, nullptr);
    EXPECT_EQ(pCmdBufferList->bufferCount(), pSwapchain->imageCount());


    // -------------------------------------------------------------------------
    // draw something
    // -------------------------------------------------------------------------
    // Wait fence valid
    EXPECT_TRUE(pFence->wait());

    // acquire image index from swapchain to choose the frame buffer.
    uint32_t imageIdx;
    EXPECT_TRUE(pSwapchain->acquireNextImageIndex(
                    pImageAvailableSemaphore.get(),
                    &imageIdx));

    // acquire frame buffer
    // auto pFramebuffer = pRenderPass->acquireFramebuffer(imageIdx);
    // EXPECT_NE(pFramebuffer, nullptr);

    // acquire command buffer
    auto pCmdBuffer = pCmdBufferList->commandBuffer(imageIdx);
    EXPECT_NE(pCmdBuffer, nullptr);

    // reset objects
    EXPECT_TRUE(pFence->reset());
    EXPECT_TRUE(pCmdBuffer->reset());

    // prepare command buffer
    cgl::Viewport viewport(0, 0, WIN_W, WIN_H);
    EXPECT_TRUE(pCmdBuffer->begin());
    {
        // pRenderPass->begin(viewport, pCmdBuffer, pFramebuffer);
        // pRenderPass->end();
    }
    EXPECT_TRUE(pCmdBuffer->end());

    // submit the rendering
    EXPECT_TRUE(pDevice->graphicsQueue()->submit(
                    pCmdBuffer,
                    pImageAvailableSemaphore.get(),
                    pRenderFinishSemaphore.get(),
                    pFence.get()));

    // present
    EXPECT_TRUE(pDevice->presentQueue()->present(
                    pRenderFinishSemaphore.get(),
                    pSwapchain.get(),
                    imageIdx));

    // release
    EXPECT_EQ(pDevice->waitIdle(), cgl::Results::Success);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
