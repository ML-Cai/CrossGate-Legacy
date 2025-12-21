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
#include "cgl/graphics/buffer_upload_manager.h"
#include "cgl/graphics/transfer_buffer.h"
#include "cgl/graphics/buffer_allocator.h"

// -----------------------------------------------------------------------------
namespace {

class IBufferUploadManagerTest : public testing::Test {
 protected:
    void SetUp() override {
        EXPECT_EQ(glfwInit(), GLFW_TRUE);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(1, 1, "test", nullptr, nullptr);
        EXPECT_NE(window_, nullptr);

        pDevice_ = cgl::graphics::IDevice::create(window_, true);
        pFence_ = cgl::graphics::IFence::create(pDevice_.get());
        pCmdBufferList_ = cgl::graphics::ICommandBufferList::create(
                            pDevice_.get(), 3);
    }

    GLFWwindow *window_;
    cgl::graphics::IDevice::Ptr pDevice_;
    cgl::graphics::IFence::Ptr pFence_;
    cgl::graphics::ICommandBufferList::Ptr pCmdBufferList_;
};

// -----------------------------------------------------------------------------
template<typename T_JOB>
bool SubmitAndWait(
    cgl::graphics::IDevice*            pDevice,
    cgl::graphics::IFence*             pFence,
    cgl::graphics::ICommandBufferList* pCmdBufferList,
    const T_JOB&                       job
) {
    if (pFence->reset() == false) {
        return false;
    }

    auto cmdBuffer = pCmdBufferList->commandBuffer(0);
    if ((cmdBuffer == nullptr) || (cmdBuffer->begin() == false)) {
        return false;
    }

    if (cgl::graphics::TransferBuffer(&job, 1, cmdBuffer) == false) {
        return false;
    }

    if (cmdBuffer->end() == false) {
        return false;
    }

    // submit
    if (pDevice->graphicsQueue()->submit(cmdBuffer, pFence) == false) {
        return false;
    }
    if (pFence->wait() == false) {
        return false;
    }

    return true;
}

}   // namespace


// -----------------------------------------------------------------------------
TEST_F(IBufferUploadManagerTest, TransferTest) {
    constexpr size_t DATA_SIZE  = 1024 * 1024;
    auto pAllocator = cgl::graphics::IBufferAllocator::create(
                        cgl::graphics::IBuffer::Types::VertexBuffer,
                        pDevice_.get(),
                        DATA_SIZE);
    auto pMgr = cgl::graphics::IBufferUploadManager::create(pDevice_.get(), 3);
    EXPECT_NE(pMgr, nullptr);

    // generate rand data
    static uint8_t data[DATA_SIZE];
    for (size_t i = 0 ; i < DATA_SIZE ; i++) {
        data[i] = rand() % 255;
    }

    // create buffer
    auto pDevBuffer = pAllocator->alloc(DATA_SIZE);
    auto pReadbackBuffer = cgl::graphics::IStagingBuffer::create(pDevice_.get(),
                                        DATA_SIZE,
                                        "test_buffer");
    EXPECT_NE(pDevBuffer, nullptr);
    EXPECT_NE(pReadbackBuffer, nullptr);

    // copy data to buffer
    {
        pFence_->reset();

        auto cmdBuffer = pCmdBufferList_->commandBuffer(0);
        cmdBuffer->begin();
        {
            pMgr->begin();
            pMgr->upload(data, DATA_SIZE, 4, pDevBuffer.get(), 0);
            pMgr->submit(cmdBuffer);
        }
        cmdBuffer->end();

        // submit
        pDevice_->graphicsQueue()->submit(cmdBuffer, pFence_.get());
        pFence_->wait();
    }


    // readback from buffer
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferReadbackJob {
                .srcBuffer = pDevBuffer.get(),
                .srcOffset = 0,
                .dstBuffer = pReadbackBuffer.get(),
                .dstOffset = 0,
                .size = DATA_SIZE
            }));

    // compare
    EXPECT_EQ(0, std::memcmp(pReadbackBuffer->data(), data, DATA_SIZE));
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
