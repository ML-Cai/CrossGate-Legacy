// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2026 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include "cgl/common/resource_cache.h"
#include "cgl/utils/filesystem.h"

namespace cgl {
inline ResourceCacheStatistics DescribeReaderCache(
    const FileBlockCache::Statistics& s, size_t scratchBytes = 0) noexcept {
    return {
        .budgetBytes = s.budgetBytes,
        .residentBytes = s.residentBytes,
        .peakResidentBytes = s.peakResidentBytes,
        .cachedBlocks = s.cachedBlocks,
        .hits = s.hits,
        .misses = s.misses,
        .evictions = s.evictions,
        .fileReadCalls = s.fileReadCalls,
        .fileReadBytes = s.fileReadBytes,
        .failedReads = s.failedReads,
        .bypassReads = s.bypassReads,
        .scratchBytes = scratchBytes
    };
}
} // namespace cgl
