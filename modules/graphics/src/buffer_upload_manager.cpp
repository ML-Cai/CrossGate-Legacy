// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <atomic>
#include <memory>
#include <vector>
#include <queue>
#include <sstream>
#include <mutex>
#include "cgl/trace/logger.h"
#include "cgl/graphics/buffer_upload_manager.h"
#include "cgl/graphics/buffer.h"
#include "cgl/graphics/staging_buffer.h"
#include "cgl/graphics/transfer_buffer.h"

// -----------------------------------------------------------------------------
namespace {

struct Chunk {
    Chunk() : head(0) {}

    cgl::graphics::IStagingBuffer::Ptr stagingBuffer;
    std::atomic<uint64_t> head;
};

struct ChunkPool {
    std::vector<std::unique_ptr<Chunk> > chunks;
    std::atomic<size_t>                  usedChunkCount;
};

using ChunkRefList = std::vector<Chunk*>;


}   // namespace

// -----------------------------------------------------------------------------
class BufferUploadManager : public cgl::graphics::IBufferUploadManager {
 public:
    using Ptr = std::unique_ptr<IBufferUploadManager>;

    struct CreateInfo {
        cgl::graphics::IDevice* pDevice;
        uint32_t                framesInFlight;
        size_t                  chunkSize;
        size_t                  maxChunks;
    };

    explicit BufferUploadManager(const CreateInfo& createInfo);

    ~BufferUploadManager();

    bool init();

    bool upload(
        const void*             pData,
        uint64_t                size,
        uint64_t                alignment,
        cgl::graphics::IBuffer* pDstBuffer,
        uint64_t                dstOffset = 0) override;

    void begin() override;

    bool submit(cgl::graphics::ICommandBuffer* pCmgBuffer) override;

 private:
    const CreateInfo createInfo_;
    const CreateInfo& createInfo() const noexcept { return createInfo_; }


    ChunkPool chunkPool_;
    void createCunkPool();

    std::atomic<std::shared_ptr<ChunkRefList> > chunks_;
    std::mutex mutex_;

    std::vector<cgl::graphics::BufferUploadJob> uploadJobs_;


    bool tryUpload(
        const void*             pData,
        uint64_t                size,
        uint64_t                alignment,
        cgl::graphics::IBuffer* pDstBuffer,
        uint64_t                dstOffset
    );

    bool allocChunkAndRetry(
        const void*             pData,
        uint64_t                size,
        uint64_t                alignment,
        cgl::graphics::IBuffer* pDstBuffer,
        uint64_t                dstOffset
    );

    bool tryUploadToChunk(
        Chunk*      pChunk,
        const void* pData,
        size_t      size,
        size_t*     pChunkAllocatedOffset
    );

    void addBufferUploadJob(
        cgl::graphics::IStagingBuffer* pStaggingBuffer,
        uint64_t                       srcOffset,
        cgl::graphics::IBuffer*        dstBuffer,
        uint64_t                       dstOffset,
        uint64_t                       size
    );
};

// -----------------------------------------------------------------------------
BufferUploadManager::BufferUploadManager(const CreateInfo& createInfo)
    : createInfo_(createInfo) {
}

// -----------------------------------------------------------------------------
BufferUploadManager::~BufferUploadManager() {
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::init() {
    if ((createInfo().pDevice == nullptr) ||
        (createInfo().framesInFlight < 1) ||
        (createInfo().chunkSize < 128) ||
        (createInfo().maxChunks < 1)) {
        LOGE("Invalid args to create buffer upload manager"
             << ", framesInFlight: " << createInfo().framesInFlight
             << ", chunkSize: " << createInfo().chunkSize
             << ", maxChunks: " << createInfo().maxChunks );
        return false;
    }

    // create chunk pool
    createCunkPool();

    return true;
}

// -----------------------------------------------------------------------------
void BufferUploadManager::createCunkPool() {
    chunkPool_.usedChunkCount = 0;
    chunkPool_.chunks.resize(createInfo().maxChunks);
    for (size_t i = 0 ; i < createInfo().maxChunks ; i++ ) {
        chunkPool_.chunks[i] = std::make_unique<Chunk>();
    }

    auto newChunks = std::make_shared<ChunkRefList>();
    chunks_.store(newChunks, std::memory_order_release);
}

// -----------------------------------------------------------------------------
void BufferUploadManager::addBufferUploadJob(
    cgl::graphics::IStagingBuffer* pStaggingBuffer,
    uint64_t                       srcOffset,
    cgl::graphics::IBuffer*        dstBuffer,
    uint64_t                       dstOffset,
    uint64_t                       size
) {
    uploadJobs_.emplace_back(pStaggingBuffer, srcOffset, dstBuffer, dstOffset, size);
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::tryUploadToChunk(
    Chunk*      pChunk,
    const void* pData,
    size_t      size,
    size_t*     pChunkAllocatedOffset
) {
    if ((size == 0) || (size > pChunk->stagingBuffer->capacity())) {
        return false;
    }

    do {
        size_t current_head = pChunk->head.load(std::memory_order_relaxed);
        size_t capacity = pChunk->stagingBuffer->capacity();

        if (capacity - current_head < size) {
            return false;
        }

        size_t next_head = (current_head + size);
        if (pChunk->head.compare_exchange_weak(
                current_head,
                next_head,
                std::memory_order_release,
                std::memory_order_relaxed)
        ) {
            LOGT("Upload " << size << " bytes, next_head: " << next_head << " to " << pChunk->stagingBuffer->name());
            pChunk->stagingBuffer->update(pData, size, current_head);
            *pChunkAllocatedOffset = current_head;
            break;
        }
    } while (true);

    return true;
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::allocChunkAndRetry(
    const void*   pData,
    uint64_t      size,
    uint64_t      alignment,
    cgl::graphics::IBuffer* pDstBuffer,
    uint64_t      dstOffset
) {
    std::lock_guard<std::mutex> lg(mutex_);

    // double check, if we chould upload the data in the moment?
    // another thread might aleardy allocated new chunk.
    if (tryUpload(pData, size, alignment, pDstBuffer, dstOffset) == true) {
        return true;
    }

    if (chunkPool_.usedChunkCount.load(std::memory_order_acquire) >= createInfo().maxChunks) {
        LOGE("No free chunks available.");
        return false;
    }

    // acquire new chunk from pool
    auto curChunks   = chunks_.load(std::memory_order_acquire);
    auto nextChunkId = chunkPool_.usedChunkCount.fetch_add(1);
    auto nextChunk   = chunkPool_.chunks[nextChunkId].get();

    if (nextChunk->stagingBuffer == nullptr) {
        size_t chunkSize = std::max(createInfo().chunkSize, size);
        std::stringstream ss;
        ss << "chunk:" << static_cast<int>(nextChunkId)
           << "/" << (createInfo().maxChunks-1) ;
        LOGD("Allocate chunk(" << ss.str().c_str() << ")");

        nextChunk->stagingBuffer = cgl::graphics::IStagingBuffer::create(
                                    createInfo().pDevice,
                                    chunkSize,
                                    ss.str().c_str());
    }

    // copy cur chunks to new chunks
    auto newChunks = std::make_shared<ChunkRefList>(*curChunks);
    newChunks->emplace_back(nextChunk);

    // swap chunks handle
    chunks_.store(newChunks, std::memory_order_release);

    return tryUpload(pData, size, alignment, pDstBuffer, dstOffset);
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::tryUpload(
    const void*   pData,
    uint64_t      size,
    uint64_t      alignment,
    cgl::graphics::IBuffer* pDstBuffer,
    uint64_t      dstOffset
) {
    // loop exist chunks to upload data
    const auto chunks = chunks_.load(std::memory_order_acquire);
    if (chunks == nullptr) {
        return false;
    }

    for (auto chunk : *chunks) {
        size_t chunkAllocatedOffset;

        if (tryUploadToChunk(chunk, pData, size, &chunkAllocatedOffset)) {
            addBufferUploadJob(
                chunk->stagingBuffer.get(),
                chunkAllocatedOffset,
                pDstBuffer,
                dstOffset,
                size
            );
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::upload(
    const void*   pData,
    uint64_t      size,
    uint64_t      alignment,
    cgl::graphics::IBuffer* pDstBuffer,
    uint64_t      dstOffset
) {
    // loop exist chnks
    if (tryUpload(pData, size, alignment, pDstBuffer, dstOffset) == true) {
        return true;
    }

    // try to allocate new chunk and retry upload (slow path)
    return allocChunkAndRetry(pData, size, alignment, pDstBuffer, dstOffset);
}

// -----------------------------------------------------------------------------
void BufferUploadManager::begin() {
    std::lock_guard<std::mutex> lg(mutex_);
    LOGW("Begin ....");

    // reset all chunks
    const auto chunks = chunks_.load(std::memory_order_acquire);
    for (auto& chunk : *chunks) {
        chunk->head = 0;
    }
}

// -----------------------------------------------------------------------------
bool BufferUploadManager::submit(cgl::graphics::ICommandBuffer* pCmgBuffer) {

    if (cgl::graphics::TransferBuffer(
            uploadJobs_.data(),
            uploadJobs_.size(),
            pCmgBuffer) == false) {
        LOGE("Failed to submit upload buffer jobs to command buffer.");
        return false;
    }

    // clean all copy jobs
    uploadJobs_.clear();
    return true;
}

// -----------------------------------------------------------------------------
 cgl::graphics::IBufferUploadManager::Ptr cgl::graphics::IBufferUploadManager::create(
    cgl::graphics::IDevice* pDevice,
    const uint32_t          framesInFlight,
    const size_t            chunkSize,
    const size_t            maxChunks
) {
    auto pMgr = std::make_unique<BufferUploadManager>(
        BufferUploadManager::CreateInfo {
            .pDevice        = pDevice,
            .framesInFlight = framesInFlight,
            .chunkSize      = chunkSize,
            .maxChunks      = maxChunks,
        }
    );

    if ((pMgr == nullptr) || (pMgr->init() == false)) {
        return nullptr;
    }
    return pMgr;
}
