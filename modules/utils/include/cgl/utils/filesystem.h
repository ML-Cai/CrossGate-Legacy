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
    FileBlockCache(size_t blockSize = 4096, size_t maxCachedBlocks = 512)
        : blockSize_(blockSize),
          maxBlocks_(maxCachedBlocks),
          fileSize_(0) {
        if (blockSize == 0 || maxCachedBlocks == 0) {
            throw std::invalid_argument("FileBlockCache requires nonzero limits");
        }
    }

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
        decltype(cache_)().swap(cache_);
        lruList_.clear();
        fileInfo_.stream.reset();
        fileInfo_.path.clear();
        fileInfo_.errorMsg.clear();
        fileSize_ = 0;
    }

    size_t cachedBlockCount() const noexcept { return cache_.size(); }

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
                        block->buffer.data() + blockOffset,
                        toRead);
            readTotal += toRead;
        }
        return true;
    }

 private:
    struct Block {
        Block(size_t size)
            : buffer(size), bufferDataSize(0) {}

        std::vector<uint8_t> buffer;
        size_t bufferDataSize;
    };


    cgl::FileInfo fileInfo_;
    size_t fileSize_;
    size_t blockSize_;
    size_t maxBlocks_;

    // LRU Cache core
    std::list<uint64_t> lruList_;  // blockId
    std::unordered_map<
        uint64_t,
        std::pair<std::shared_ptr<Block>,
                  std::list<uint64_t>::iterator> > cache_;

    std::shared_ptr<Block> getBlock(uint64_t blockId) {
        auto iter = cache_.find(blockId);
        if (iter != cache_.end()) {
            // Update LRU
            lruList_.splice(lruList_.begin(), lruList_, iter->second.second);
            return iter->second.first;
        }

        // Read from file if the cache doesn't exist
        const size_t offset = blockId * blockSize_;
        auto newBlock = std::make_shared<Block>(blockSize_);
        size_t availableBytes = (offset >= fileSize_) ? 0 : std::min(blockSize_, fileSize_ - offset);
        if (availableBytes == 0) {
            return nullptr;
        }
        try {
            fileInfo_.stream->clear();
            fileInfo_.stream->seekg(offset, std::ios::beg);
            fileInfo_.stream->read(reinterpret_cast<char*>(newBlock->buffer.data()),
                                   availableBytes);
        } catch (const std::ios_base::failure&) {
            return nullptr; // Do not cache an incomplete block.
        }
        newBlock->bufferDataSize = availableBytes;

        // retire cache
        if (cache_.size() >= maxBlocks_) {
            uint64_t oldBlockId = lruList_.back();
            lruList_.pop_back();
            cache_.erase(oldBlockId);
        }

        // add new cache
        lruList_.push_front(blockId);
        try {
            cache_.emplace(blockId, std::make_pair(newBlock, lruList_.begin()));
        } catch (...) {
            lruList_.pop_front();
            throw;
        }

        return newBlock;
    }
};



}   // namespace cgl
