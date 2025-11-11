// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <vulkan/vulkan.h>
#include "cgl/trace/logger.h"
#include "cgl/graphics/command_buffer_list.h"
#include "cgl/graphics/transfer_buffer.h"
#include "vulkan/command_buffer_list.h"
#include "vulkan/buffer.h"
#include "vulkan/staging_buffer.h"


// -----------------------------------------------------------------------------
namespace {

template<typename T_JOB, typename T_BUFFER_1, typename T_BUFFER_2>
bool TransferBuffer(
    const T_JOB*          pJobs,
    size_t                jobCount,
    cgl::ICommandBuffer*  pCmdBuffer
) {
    if ((pJobs == nullptr) || (pCmdBuffer == nullptr)) {
        return false;
    }
    auto pVkCmdBuffer = static_cast<cgl::vk::CommandBuffer*>(pCmdBuffer);

    for (size_t i = 0 ; i < jobCount ; i++) {
        VkBufferCopy copyRegion {
            .srcOffset = pJobs[i].srcOffset,
            .dstOffset = pJobs[i].dstOffset,
            .size      = pJobs[i].size
        };

        auto pVkSrcBuffer = static_cast<T_BUFFER_1*>(pJobs[i].srcBuffer);
        auto pVkDstBuffer = static_cast<T_BUFFER_2*>(pJobs[i].dstBuffer);

        LOGD("Copy buffer " << pVkSrcBuffer->buffer() << " to " << pVkDstBuffer->buffer()
             << " , region [srcOffset: " << copyRegion.srcOffset
             << ", dstOffset: " << copyRegion.dstOffset
             << ", size: " << copyRegion.size
             << "]");

        vkCmdCopyBuffer(pVkCmdBuffer->commandBuffer(),
                        pVkSrcBuffer->buffer(),
                        pVkDstBuffer->buffer(),
                        1, &copyRegion);
    }


    return true;
}

}   // namespace

// -----------------------------------------------------------------------------
bool cgl::TransferBuffer(
    const cgl::BufferUploadJob* pJobs,
    size_t                jobCount,
    cgl::ICommandBuffer*  pCmdBuffer
) {
    return ::TransferBuffer<
                cgl::BufferUploadJob,
                cgl::vk::StagingBuffer,
                cgl::vk::Buffer>(
                    pJobs,
                    jobCount,
                    pCmdBuffer);
}

// -----------------------------------------------------------------------------
bool cgl::TransferBuffer(
    const cgl::BufferReadbackJob* pJobs,
    size_t                  jobCount,
    cgl::ICommandBuffer*    pCmdBuffer
) {
    return ::TransferBuffer<
                cgl::BufferReadbackJob,
                cgl::vk::Buffer,
                cgl::vk::StagingBuffer>(
                    pJobs,
                    jobCount,
                    pCmdBuffer);
}
