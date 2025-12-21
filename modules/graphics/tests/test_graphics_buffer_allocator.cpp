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
#include "cgl/graphics/device.h"
#include "cgl/graphics/queue.h"
#include "cgl/graphics/buffer.h"
#include "cgl/graphics/fence.h"
#include "cgl/graphics/staging_buffer.h"
#include "cgl/graphics/command_buffer_list.h"
#include "cgl/graphics/buffer_allocator.h"

// -----------------------------------------------------------------------------
namespace {

class IBufferAllocatorTest : public testing::Test {
 protected:
    void SetUp() override {
        EXPECT_EQ(glfwInit(), GLFW_TRUE);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(1, 1, "test", nullptr, nullptr);
        EXPECT_NE(window_, nullptr);

        pDevice_ = cgl::graphics::IDevice::create(window_, true);
        pFence_  = cgl::graphics::IFence::create(pDevice_.get());
        pCmdBufferList_ = cgl::graphics::ICommandBufferList::create(
                            pDevice_.get(), 3);
    }

    GLFWwindow *window_;
    cgl::graphics::IDevice::Ptr pDevice_;
    cgl::graphics::IFence::Ptr pFence_;
    cgl::graphics::ICommandBufferList::Ptr pCmdBufferList_;
};

}   // namespace


// -----------------------------------------------------------------------------
TEST_F(IBufferAllocatorTest, AllocTest) {
    constexpr size_t TEST_COUNT = 3;
    constexpr size_t DATA_SIZE  = 1024 * 1024;
    auto pAllocator = cgl::graphics::IBufferAllocator::create(
                        cgl::graphics::IBuffer::Types::VertexBuffer,
                        pDevice_.get(),
                        TEST_COUNT * DATA_SIZE);
    EXPECT_NE(pAllocator, nullptr);
    EXPECT_EQ(pAllocator->bufferType(), cgl::graphics::IBuffer::Types::VertexBuffer);
    EXPECT_EQ(pAllocator->poolSize(), TEST_COUNT * DATA_SIZE);

    // alloc buffer from allocator
    for (size_t i = 0 ; i < (TEST_COUNT + 1) ; i++) {
        auto pDevBuffer = pAllocator->alloc(DATA_SIZE);
        if (i >= TEST_COUNT) {
            EXPECT_EQ(pDevBuffer, nullptr);
        } else {
            EXPECT_NE(pDevBuffer, nullptr);
            EXPECT_EQ(pDevBuffer->type(), pAllocator->bufferType());
            EXPECT_EQ(pDevBuffer->capacity(), DATA_SIZE);
        }
    }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
