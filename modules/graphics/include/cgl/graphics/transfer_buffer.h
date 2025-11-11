// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>

namespace cgl {

class ICommandBuffer;
class IBuffer;
class IStagingBuffer;

// -----------------------------------------------------------------------------
struct BufferUploadJob {
    cgl::IStagingBuffer* srcBuffer;
    uint64_t             srcOffset;
    cgl::IBuffer*        dstBuffer;
    uint64_t             dstOffset;
    uint64_t             size;
};

struct BufferReadbackJob {
    cgl::IBuffer*        srcBuffer;
    uint64_t             srcOffset;
    cgl::IStagingBuffer* dstBuffer;
    uint64_t             dstOffset;
    uint64_t             size;
};

bool TransferBuffer(const cgl::BufferUploadJob* pJobs, size_t jobCount,
                    cgl::ICommandBuffer* pCmdBuffer);

bool TransferBuffer(const cgl::BufferReadbackJob* pJobs, size_t jobCount,
                    cgl::ICommandBuffer* pCmdBuffer);


// -----------------------------------------------------------------------------
}   // namespace cgl
