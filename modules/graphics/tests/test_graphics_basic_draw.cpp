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
#include <vector>
#include "cgl/common/viewport.h"
#include "cgl/graphics/swapchain.h"
#include "cgl/graphics/queue.h"
#include "cgl/graphics/fence.h"
#include "cgl/graphics/semaphore.h"
#include "cgl/graphics/command_buffer_list.h"
#include "cgl/graphics/render_pass.h"
#include "vulkan/device.h"

namespace {
namespace graphics = cgl::graphics;
constexpr uint32_t side = 256;

class BasicDrawTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(glfwInit(), GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(side, side, "basic draw test", nullptr, nullptr);
        ASSERT_NE(window_, nullptr);
        device_ = std::make_unique<graphics::vulkan::Device>(true);
        ASSERT_TRUE(device_->prepare(window_));
    }
    void TearDown() override {
        // Fixture-owned children survive fatal assertions until submitted work
        // has finished. Then destroy in dependency order, before the window.
        if(device_ && device_->device()!=VK_NULL_HANDLE)
            EXPECT_EQ(device_->waitIdle(), cgl::Results::Success);
        commands_.reset();pass_.reset();fence_.reset();
        available_.reset();finished_.clear();swapchain_.reset();
        if(device_) {
            device_->destroy(); // Includes device-child leak diagnostics.
            EXPECT_EQ(device_->validationErrorCount(),0u);
            device_.reset();
        }
        if(window_) {glfwDestroyWindow(window_);window_=nullptr;}
        glfwTerminate();
    }
    GLFWwindow* window_=nullptr;
    std::unique_ptr<graphics::vulkan::Device> device_;
    graphics::ISwapchain::Ptr swapchain_;
    graphics::IRenderPass::Ptr pass_;
    graphics::ICommandBufferList::Ptr commands_;
    graphics::IFence::Ptr fence_;
    graphics::ISemaphore::Ptr available_;
    std::vector<graphics::ISemaphore::Ptr> finished_;
};
}

TEST_F(BasicDrawTest, BasicDrawFlow) {
    swapchain_=graphics::ISwapchain::create(device_.get(),window_);
    ASSERT_NE(swapchain_,nullptr);ASSERT_GT(swapchain_->imageCount(),0u);
    EXPECT_EQ(swapchain_->extent().width,side);EXPECT_EQ(swapchain_->extent().height,side);
    pass_=graphics::IRenderPass::create(graphics::IRenderPass::BuiltinKeys::InitScene,device_.get(),swapchain_.get());
    ASSERT_NE(pass_,nullptr);
    commands_=graphics::ICommandBufferList::create(device_.get(),swapchain_->imageCount());
    ASSERT_NE(commands_,nullptr);ASSERT_EQ(commands_->bufferCount(),swapchain_->imageCount());
    fence_=graphics::IFence::create(device_.get());ASSERT_NE(fence_,nullptr);
    available_=graphics::ISemaphore::create(device_.get());ASSERT_NE(available_,nullptr);
    for(uint32_t i=0;i<swapchain_->imageCount();++i) {
        finished_.push_back(graphics::ISemaphore::create(device_.get()));
        ASSERT_NE(finished_.back(),nullptr);
    }
    const cgl::Viewport viewport(0,0,side,side);
    for(uint32_t frame=0;frame<2*swapchain_->imageCount()+1;++frame) {
        SCOPED_TRACE(frame);
        ASSERT_TRUE(fence_->wait());
        uint32_t image=0;
        ASSERT_TRUE(swapchain_->acquireNextImageIndex(available_.get(),&image));
        ASSERT_LT(image,swapchain_->imageCount());
        auto* command=commands_->commandBuffer(image);
        auto* framebuffer=pass_->acquireFramebuffer(image);
        ASSERT_NE(command,nullptr);ASSERT_NE(framebuffer,nullptr);
        ASSERT_TRUE(command->reset());ASSERT_TRUE(command->begin());
        // The real color pass clears the acquired image and transitions it to
        // PRESENT_SRC_KHR. An empty command buffer cannot validate this flow.
        ASSERT_TRUE(pass_->begin(viewport,command,framebuffer));
        ASSERT_TRUE(pass_->end());ASSERT_TRUE(command->end());
        ASSERT_TRUE(fence_->reset());
        ASSERT_TRUE(device_->graphicsQueue()->submit(command,available_.get(),finished_[image].get(),fence_.get()));
        // Per-image semaphores: reacquiring that image establishes completion of
        // the preceding presentation before this semaphore is signalled again.
        ASSERT_TRUE(device_->presentQueue()->present(finished_[image].get(),swapchain_.get(),image));
    }
    ASSERT_EQ(device_->waitIdle(),cgl::Results::Success);
    EXPECT_EQ(device_->validationErrorCount(),0u);
}
