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

class TransferBufferTest : public testing::Test {
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

void GenTestData(uint8_t *pData, size_t size) {
    for (size_t i = 0 ; i < size ; i++) {
        pData[i] = rand() % 255;
    }
}

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
TEST_F(TransferBufferTest, TransferTest) {
    // generate rand data
    constexpr size_t DATA_SIZE = 1024 * 1024;
    static uint8_t data[DATA_SIZE];
    GenTestData(data, DATA_SIZE);

    // create buffer
    auto pDevice = pDevice_.get();
    auto pAllocator = cgl::graphics::IBufferAllocator::create(
                        cgl::graphics::IBuffer::Types::VertexBuffer,
                        pDevice,
                        DATA_SIZE);
    auto pUploadBuffer   = cgl::graphics::IStagingBuffer::create(pDevice, DATA_SIZE);
    auto pReadbackBuffer = cgl::graphics::IStagingBuffer::create(pDevice, DATA_SIZE);
    auto pDevBuffer      = pAllocator->alloc(DATA_SIZE);

    // copy data to upload buffer
    EXPECT_EQ(pUploadBuffer->capacity(), DATA_SIZE);
    EXPECT_EQ(pUploadBuffer->update(data, DATA_SIZE, 0), true);

    // copy upload buffer to device buffer
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferUploadJob {
                .srcBuffer = pUploadBuffer.get(),
                .srcOffset = 0,
                .dstBuffer = pDevBuffer.get(),
                .dstOffset = 0,
                .size = DATA_SIZE
            }));

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
TEST_F(TransferBufferTest, PartialUploadTest) {
    // generate rand data
    constexpr size_t DATA_SIZE = 1024 * 1024;
    constexpr size_t COPY_SIZE = DATA_SIZE / 2;
    static uint8_t data[DATA_SIZE];
    GenTestData(data, DATA_SIZE);

    // create buffer
    auto pDevice = pDevice_.get();
    auto pAllocator = cgl::graphics::IBufferAllocator::create(
                        cgl::graphics::IBuffer::Types::VertexBuffer,
                        pDevice,
                        DATA_SIZE);
    auto pUploadBuffer   = cgl::graphics::IStagingBuffer::create(pDevice, DATA_SIZE);
    auto pReadbackBuffer = cgl::graphics::IStagingBuffer::create(pDevice, DATA_SIZE);
    auto pDevBuffer      = pAllocator->alloc(DATA_SIZE);

    // copy data to upload buffer
    EXPECT_NE(pDevBuffer, nullptr);
    EXPECT_EQ(pUploadBuffer->capacity(), DATA_SIZE);
    EXPECT_EQ(pUploadBuffer->update(data, DATA_SIZE, 0), true);

    // copy half upload buffer to device buffer
    //  *---*       *---*
    //  | A |       |   |
    //  *---*  >>>  *---*
    //  | B |       | A |
    //  *---*       *---*
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferUploadJob {
                .srcBuffer = pUploadBuffer.get(),
                .srcOffset = 0,
                .dstBuffer = pDevBuffer.get(),
                .dstOffset = COPY_SIZE,
                .size      = COPY_SIZE
            }));

    // readback from buffer
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferReadbackJob {
                .srcBuffer = pDevBuffer.get(),
                .srcOffset = 0,
                .dstBuffer = pReadbackBuffer.get(),
                .dstOffset = 0,
                .size      = DATA_SIZE
            }));

    // compare
    EXPECT_EQ(0, std::memcmp(
        reinterpret_cast<uint8_t *>(pReadbackBuffer->data()) + COPY_SIZE,
        data, COPY_SIZE));



    // copy half upload buffer to device buffer
    //  *---*       *---*
    //  | A |       | B |
    //  *---*  >>>  *---*
    //  | B |       | A |
    //  *---*       *---*
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferUploadJob {
                .srcBuffer = pUploadBuffer.get(),
                .srcOffset = COPY_SIZE,
                .dstBuffer = pDevBuffer.get(),
                .dstOffset = 0,
                .size      = COPY_SIZE
            }));

    // readback from buffer
    EXPECT_TRUE(
        SubmitAndWait(pDevice_.get(), pFence_.get(), pCmdBufferList_.get(),
            cgl::graphics::BufferReadbackJob {
                .srcBuffer = pDevBuffer.get(),
                .srcOffset = 0,
                .dstBuffer = pReadbackBuffer.get(),
                .dstOffset = 0,
                .size      = DATA_SIZE
            }));

    EXPECT_EQ(0, std::memcmp(
        reinterpret_cast<uint8_t *>(pReadbackBuffer->data()) + COPY_SIZE,
        data, COPY_SIZE));

    EXPECT_EQ(0, std::memcmp(
        reinterpret_cast<uint8_t *>(pReadbackBuffer->data()),
        data + COPY_SIZE, COPY_SIZE));
}


// -----------------------------------------------------------------------------
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
