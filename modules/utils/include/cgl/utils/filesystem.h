// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <assert.h>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <limits>
#include <memory>
#include <fstream>
#include <filesystem>
#include <optional>
#include <utility>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>

namespace cgl {

//------------------------------------------------------------------------------
struct FileInfo {
    std::filesystem::path path;
    std::optional<std::ifstream> stream;
    std::string errorMsg;

    bool isOpen() const {
        return stream.has_value() && stream.value().is_open();
    }
};

//------------------------------------------------------------------------------
inline cgl::FileInfo TryOpenBinaryFile(
    const std::filesystem::path& filePath
) {
    std::ifstream file;

    try {
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(filePath, std::ios::in | std::ios::binary);
    } catch (const std::ios_base::failure& e) {
        return { filePath, std::nullopt, e.what() };
    }

    return { filePath, std::move(file), {} };
}

//------------------------------------------------------------------------------
inline size_t GetFileSize(cgl::FileInfo* pOpenInfo) {
    assert(pOpenInfo->stream.has_value());

    auto& f = pOpenInfo->stream.value();
    f.seekg(0, f.end);
    size_t bufferSize = static_cast<size_t>(f.tellg());
    f.seekg(0, f.beg);

    return bufferSize;
}

//------------------------------------------------------------------------------
class FileBlockCache {
 public:
    struct Statistics {
        size_t budgetBytes = 0;
        size_t residentBytes = 0;
        size_t peakResidentBytes = 0;
        size_t cachedBlocks = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t evictions = 0;
        uint64_t fileReadCalls = 0;
        uint64_t fileReadBytes = 0;
        uint64_t failedReads = 0;
        uint64_t bypassReads = 0;
    };

    FileBlockCache(size_t blockSize = 4096, size_t maxCachedBlocks = 512)
        : fileSize_(0), blockSize_(blockSize) {
        if (blockSize == 0 || maxCachedBlocks == 0) {
            throw std::invalid_argument("FileBlockCache requires nonzero limits");
        }
        if (maxCachedBlocks > std::numeric_limits<size_t>::max() / blockSize) {
            throw std::invalid_argument("FileBlockCache byte budget overflow");
        }
        stats_.budgetBytes = blockSize * maxCachedBlocks;
    }

    FileBlockCache(const FileBlockCache&) = delete;
    FileBlockCache& operator=(const FileBlockCache&) = delete;
    virtual ~FileBlockCache() = default;

    virtual bool open(const std::filesystem::path& filePath) {
        if (isStreamReady() && fileInfo_.path == filePath) return true;
        close();
        fileInfo_ = TryOpenBinaryFile(filePath);
        if (!fileInfo_.isOpen()) return false;
        try {
            fileSize_ = cgl::GetFileSize(&fileInfo_);
            return true;
        } catch (const std::ios_base::failure&) {
            close();
            return false;
        }
    }

    // Single-owner, synchronous cache. Callers must serialize access, including close.
    // Destruction also releases the file and all cached blocks through RAII.
    virtual void close() noexcept {
        trimCache();
        fileInfo_.stream.reset();
        fileInfo_.path.clear();
        fileInfo_.errorMsg.clear();
        fileSize_ = 0;
    }

    size_t cachedBlockCount() const noexcept { return cache_.size(); }

    // Budgets cover block payload allocations, excluding container/stream overhead.
    // A budget smaller than one block bypasses the cache without scratch allocation.
    void setCacheBudget(size_t bytes) noexcept {
        stats_.budgetBytes = bytes;
        while (cache_.size() > bytes / blockSize_) evictOldest();
        if (cache_.empty()) decltype(cache_)().swap(cache_);
    }

    void trimCache() noexcept {
        stats_.evictions += cache_.size();
        decltype(cache_)().swap(cache_);
        lruList_.clear();
        stats_.residentBytes = 0;
    }

    Statistics cacheStatistics() const noexcept {
        auto result = stats_;
        result.cachedBlocks = cache_.size();
        return result;
    }

    // Lifetime counters survive close/open; resetting keeps current occupancy.
    void resetCacheStatistics() noexcept {
        const auto budget = stats_.budgetBytes;
        const auto resident = stats_.residentBytes;
        stats_ = {};
        stats_.budgetBytes = budget;
        stats_.residentBytes = stats_.peakResidentBytes = resident;
    }


    virtual bool isStreamReady() const {
        return fileInfo_.isOpen();
    }

    virtual size_t fileSize() const noexcept { return fileSize_; }

    virtual bool read(size_t offset, size_t len, void* pDest) {
        // arg check
        if (!isStreamReady()) return false;

        if ((len > fileSize_) || (offset > fileSize_) ||
            (offset > (fileSize_ - len)) || (pDest == nullptr)) {
            return false;
        }

        if (len == 0) return true;
        if (stats_.budgetBytes < blockSize_) {
            ++stats_.bypassReads;
            return readFile(offset, len, pDest);
        }
        size_t readTotal = 0;
        while (readTotal < len) {
            size_t absOffset   = offset + readTotal;
            size_t blockId     = absOffset / blockSize_;
            size_t blockOffset = absOffset % blockSize_;

            auto block = getBlock(blockId);
            if (block == nullptr || blockOffset >= block->bufferDataSize) {
                return false;
            }

            size_t toRead = std::min(block->bufferDataSize - blockOffset,
                                     len - readTotal);
            std::memcpy(reinterpret_cast<uint8_t *>(pDest) + readTotal,
                        block->buffer.get() + blockOffset,
                        toRead);
            readTotal += toRead;
        }
        return true;
    }

 private:
    struct Block {
        Block(size_t size)
            : buffer(std::make_unique<uint8_t[]>(size)), bufferDataSize(0) {}

        std::unique_ptr<uint8_t[]> buffer;
        size_t bufferDataSize;
    };


    cgl::FileInfo fileInfo_;
    size_t fileSize_;
    size_t blockSize_;
    Statistics stats_;

    // LRU Cache core
    std::list<uint64_t> lruList_;  // blockId
    std::unordered_map<
        uint64_t,
        std::pair<std::unique_ptr<Block>,
                  std::list<uint64_t>::iterator> > cache_;

    bool readFile(size_t offset, size_t bytes, void* dest) {
        ++stats_.fileReadCalls;
        try {
            fileInfo_.stream->clear();
            fileInfo_.stream->seekg(offset, std::ios::beg);
        } catch (const std::ios_base::failure&) {
            ++stats_.failedReads;
            return false;
        }
        try {
            fileInfo_.stream->read(static_cast<char*>(dest), bytes);
            stats_.fileReadBytes += bytes;
            return true;
        } catch (const std::ios_base::failure&) {
            stats_.fileReadBytes += static_cast<uint64_t>(fileInfo_.stream->gcount());
            ++stats_.failedReads;
            return false;
        }
    }

    void evictOldest() noexcept {
        const auto id = lruList_.back();
        cache_.erase(id);
        lruList_.pop_back();
        stats_.residentBytes -= blockSize_;
        ++stats_.evictions;
    }

    // Borrowed pointer is consumed immediately by read(), before the next lookup.
    const Block* getBlock(uint64_t blockId) {
        auto iter = cache_.find(blockId);
        if (iter != cache_.end()) {
            ++stats_.hits;
            lruList_.splice(lruList_.begin(), lruList_, iter->second.second);
            return iter->second.first.get();
        }
        ++stats_.misses;
        const size_t offset = blockId * blockSize_;
        const size_t availableBytes = (offset >= fileSize_) ? 0 : std::min(blockSize_, fileSize_ - offset);
        if (availableBytes == 0) return nullptr;
        // Evict before allocating to avoid a transient extra block over budget.
        while (cache_.size() >= stats_.budgetBytes / blockSize_) evictOldest();
        auto newBlock = std::make_unique<Block>(blockSize_);
        if (!readFile(offset, availableBytes, newBlock->buffer.get())) return nullptr;
        newBlock->bufferDataSize = availableBytes;
        const auto* result = newBlock.get();
        lruList_.push_front(blockId);
        try {
            cache_.emplace(blockId, std::make_pair(std::move(newBlock), lruList_.begin()));
        } catch (...) {
            lruList_.pop_front();
            throw;
        }
        stats_.residentBytes += blockSize_;
        stats_.peakResidentBytes = std::max(stats_.peakResidentBytes, stats_.residentBytes);
        return result;
    }
};

}   // namespace cgl
