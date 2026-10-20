// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2026 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace cgl {

// CPU file-cache payload only; excludes container/stream overhead and returned data.
// Counters are cumulative until resetCacheStatistics(), including across close/load.
struct ResourceCacheStatistics {
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
    size_t scratchBytes = 0; // Retained graphics decoding scratch; outside cache budget.
};

} // namespace cgl
