// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>
#include "cgl/common/version.h"
#include "cgl/common/results.h"
#include "cgl/common/assets.h"
#include "cgl/common/resource_cache.h"

namespace cgl {

struct Settings;

class IGraphicsDataReader {
 public:
    using Ptr = std::unique_ptr<cgl::IGraphicsDataReader>;

    struct CreateInfo {
        const cgl::Settings*  pSettings;
        cgl::CrossGateVersion version;
        size_t cacheBudgetBytes = 4 * 1024 * 1024;
    };

    static cgl::IGraphicsDataReader::Ptr create(const CreateInfo& createInfo);

    explicit IGraphicsDataReader(const CreateInfo& createInfo)
        : createInfo_(createInfo) {}

    virtual ~IGraphicsDataReader() = default;

    const CreateInfo& createInfo() const noexcept {
        return createInfo_;
    }

    virtual cgl::Results load() = 0;

    // Serialized access only. Returned pixels remain caller-owned after close.
    virtual void close() noexcept = 0;

    // Applies immediately; budgets smaller than a block bypass caching.
    virtual void setCacheBudget(size_t bytes) noexcept = 0;
    // Releases cached blocks and decode scratch while keeping the file open.
    virtual void trimCache() noexcept = 0;
    virtual cgl::ResourceCacheStatistics cacheStatistics() const noexcept = 0;
    virtual void resetCacheStatistics() noexcept = 0;

    virtual cgl::Results query(
        const cgl::GraphicsResourceInfo& gfxResInfo,
        cgl::GraphicsResourceData*       pGfxResData) = 0;

 private:
    const CreateInfo createInfo_;
};

}   // namespace cgl
