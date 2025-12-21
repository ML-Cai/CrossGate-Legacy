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

class IDevice;
class IBuffer;
class ICommandBuffer;

// -----------------------------------------------------------------------------
class IBufferUploadManager {
 public:
    using Ptr = std::unique_ptr<IBufferUploadManager>;

    static IBufferUploadManager::Ptr create(
        cgl::graphics::IDevice* pDevice,
        const uint32_t          framesInFlight,
        const size_t            chunkSize = 16 * 1024 * 1024,
        const size_t            maxChunks = 16);

    IBufferUploadManager() = default;

    virtual ~IBufferUploadManager() = default;

    virtual bool upload(
        const void*             pData,
        uint64_t                size,
        uint64_t                alignment,
        cgl::graphics::IBuffer* pDstBuffer,
        uint64_t                dstOffset = 0) = 0;

    // call before frame start
    virtual void begin() = 0;

    // call after frame end
    virtual bool submit(cgl::graphics::ICommandBuffer* pCmgBuffer) = 0;
};


// -----------------------------------------------------------------------------
}   // namespace graphics
}   // namespace cgl
