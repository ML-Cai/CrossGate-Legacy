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
namespace graphics {

class IBuffer;
class ICommandBuffer;
class IStagingBuffer;

// -----------------------------------------------------------------------------
struct BufferUploadJob {
    cgl::graphics::IStagingBuffer* srcBuffer;
    uint64_t                       srcOffset;
    cgl::graphics::IBuffer*        dstBuffer;
    uint64_t                       dstOffset;
    uint64_t                       size;
};

struct BufferReadbackJob {
    cgl::graphics::IBuffer*        srcBuffer;
    uint64_t                       srcOffset;
    cgl::graphics::IStagingBuffer* dstBuffer;
    uint64_t                       dstOffset;
    uint64_t                       size;
};

bool TransferBuffer(const cgl::graphics::BufferUploadJob* pJobs,
                    size_t jobCount,
                    cgl::graphics::ICommandBuffer* pCmdBuffer);

bool TransferBuffer(const cgl::graphics::BufferReadbackJob* pJobs,
                    size_t jobCount,
                    cgl::graphics::ICommandBuffer* pCmdBuffer);


// -----------------------------------------------------------------------------
}   // namespace graphics
}   // namespace cgl
