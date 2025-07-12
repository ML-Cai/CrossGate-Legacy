// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <assert.h>
#include <memory>
#include <fstream>
#include <filesystem>
#include <optional>
#include <utility>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include "cgl/core/results.h"

namespace cgl {

//------------------------------------------------------------------------------
struct FileOpenInfo {
    std::filesystem::path path;
    std::optional<std::ifstream> stream;
    cgl::Results result;
    std::string errorMsg;
};

//------------------------------------------------------------------------------
cgl::FileOpenInfo TryOpenBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream file;

    try {
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(filePath, std::ios::in | std::ios::binary);
    } catch (const std::ios_base::failure& e) {
        return { filePath, std::nullopt, cgl::Results::InvalidFile, e.what() };
    }

    return { filePath, std::move(file), cgl::Results::Success, {} };
}

//------------------------------------------------------------------------------
inline bool IsFileOpen(const cgl::FileOpenInfo& openInfo) {
    return openInfo.stream.has_value() &&
           openInfo.stream.value().is_open();
}

//------------------------------------------------------------------------------
inline size_t GetFileSize(cgl::FileOpenInfo* pOpenInfo) {
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
          maxBlocks_(maxCachedBlocks) {
    }

    virtual ~FileBlockCache() {
    }

    virtual cgl::Results open(const std::filesystem::path& filePath) {
        if (cgl::IsFileOpen(fileInfo_)) {
            return cgl::Results::Success;
        }

        fileInfo_ = TryOpenBinaryFile(filePath);

        if (fileInfo_.result == cgl::Results::Success) {
            fileSize_ = cgl::GetFileSize(&fileInfo_);
        }

        return fileInfo_.result;
    }

    virtual cgl::Results read(size_t offset, size_t len, uint8_t* pDest) {
        // arg check
        if (!IsFileOpen(fileInfo_)) return cgl::Results::InvalidArgs;
        if (pDest == nullptr) return cgl::Results::InvalidArgs;
        if ((len > fileSize_) || (offset > fileSize_) ||
            (offset > (fileSize_ - len))) {
            return cgl::Results::InvalidArgs;
        }

        size_t readTotal = 0;
        while (readTotal < len) {
            size_t absOffset   = offset + readTotal;
            size_t blockId     = absOffset / blockSize_;
            size_t blockOffset = absOffset % blockSize_;

            auto block = getBlock(blockId);
            if (block == nullptr) {
                return cgl::Results::Fail;
            }

            size_t toRead = std::min(block->bufferDataSize - blockOffset,
                                     len - readTotal);
            std::memcpy(pDest + readTotal,
                        block->buffer.data() + blockOffset,
                        toRead);
            readTotal += toRead;
        }
        return cgl::Results::Success;
    }

 private:
    struct Block {
        Block(size_t size)
            : buffer(size), bufferDataSize(0) {}

        std::vector<uint8_t> buffer;
        size_t bufferDataSize;
    };


    cgl::FileOpenInfo fileInfo_;
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
            lruList_.erase(iter->second.second);
            lruList_.push_front(blockId);
            iter->second.second = lruList_.begin();
            return iter->second.first;
        }

        // Read from file if the cache doesn't exist
        const size_t offset = blockId * blockSize_;
        auto newBlock = std::make_shared<Block>(blockSize_);
        fileInfo_.stream->seekg(offset, std::ios::beg);

        size_t availableBytes = (offset >= fileSize_) ? 0 : std::min(blockSize_, fileSize_ - offset);
        if (availableBytes == 0) {
            return nullptr;
        }
        fileInfo_.stream->read(reinterpret_cast<char*>(newBlock->buffer.data()),
                               availableBytes);

        if (!(*fileInfo_.stream)) return nullptr;
        newBlock->bufferDataSize = availableBytes;

        // retire cache
        if (cache_.size() >= maxBlocks_) {
            uint64_t oldBlockId = lruList_.back();
            lruList_.pop_back();
            cache_.erase(oldBlockId);
        }

        // add new cache
        lruList_.push_front(blockId);
        cache_[blockId] = { newBlock, lruList_.begin() };

        return newBlock;
    }
};



}   // namespace cgl
