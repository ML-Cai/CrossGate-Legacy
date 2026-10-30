// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <array>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <memory>
#include <new>
#include "cgl/common/formatters.h"
#include "cgl/assets/anime_info_reader.h"
#include "cgl/settings/settings.h"
#include "cgl/utils/filesystem.h"
#include "cgl/trace/logger.h"
#include "assets_path_info.h"

using cgl::IAnimeInfoReader;

// -----------------------------------------------------------------------------
// anonymous namespace
// -----------------------------------------------------------------------------
namespace {

#pragma pack(push, 1)
struct AnimeInfoRawData {
    uint32_t index;
    uint32_t address;
    uint16_t motionCount;
    uint16_t unknown;
};
#pragma pack(pop)


class ReaderImpl : public cgl::IAnimeInfoReader {
 public:
    explicit ReaderImpl(
        const cgl::IAnimeInfoReader::CreateInfo& createInf);

    virtual ~ReaderImpl();

    cgl::Results load() override;

    cgl::Results query(
        const cgl::AnimeResourceSerialNum& index,
        cgl::AnimeResourceInfo*            pInfo) override;

    cgl::Results queryAvailableSerialNums(
        std::vector<cgl::AnimeResourceSerialNum>* pList) noexcept override;

    void close() noexcept override;

 private:

    cgl::Results preLoad(size_t fileSize);

    cgl::FileInfo fileInfo_;

    std::unordered_map<uint32_t, cgl::AnimeResourceInfo> map_;
};

}   // namespace


// -----------------------------------------------------------------------------
// AnimeInfoRawData Impl
// -----------------------------------------------------------------------------
ReaderImpl::ReaderImpl(
    const cgl::IAnimeInfoReader::CreateInfo& createInfo)
    : cgl::IAnimeInfoReader(createInfo) {
}

// -----------------------------------------------------------------------------
ReaderImpl::~ReaderImpl() {
    close();
}

// -----------------------------------------------------------------------------
void ReaderImpl::close() noexcept {
    fileInfo_.stream.reset();
    fileInfo_.path.clear();
    fileInfo_.errorMsg.clear();
    decltype(map_)().swap(map_);
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::load() try {
    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPaths  = cgl::AcquireCrossGateResourcePath(createInfo().version);
    if ((resPaths.version == cgl::CrossGateVersion::UNKNOWN) ||
        (resPaths.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version `"
              << createInfo().version << "`");
        return cgl::Results::Fail;
    }

    close();

    // load file
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->general.EngineRootPath) /
        std::filesystem::path(resPaths.animeIndexSubPath);
    LOGD("Load graphics index from file `" << fullPath.string() << "`");

    fileInfo_ = cgl::TryOpenBinaryFile(fullPath);
    if (fileInfo_.isOpen() == false) {
        LOGE("Failed to open anime index file " << fullPath.string()
             << ", msg " << fileInfo_.errorMsg);
        return cgl::Results::InvalidFile;
    }

    // verify the size
    size_t fileSize = GetFileSize(&fileInfo_);
    if (createInfo().maxIndexFileBytes == 0 || createInfo().maxIndexEntries == 0 ||
        fileSize > createInfo().maxIndexFileBytes ||
        fileSize / sizeof(AnimeInfoRawData) > createInfo().maxIndexEntries ||
        fileSize % sizeof(AnimeInfoRawData) != 0) {
        LOGE("Invalid or oversized anime index: bytes=" << fileSize);
        close();
        return cgl::Results::InvalidFile;
    }

    // Keep only a small parsing batch while constructing the bounded index.
    auto result = preLoad(fileSize);
    if (result != cgl::Results::Success) close();
    else fileInfo_.stream.reset(); // The parsed index owns all required data.
    return result;
}
catch (const std::ios_base::failure&) {
    close();
    return cgl::Results::InvalidFile;
} catch (const std::bad_alloc&) {
    close();
    return cgl::Results::Fail;
} catch (...) {
    close();
    throw;
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::preLoad(size_t fileSize) {
    const size_t count = fileSize / sizeof(AnimeInfoRawData);
    map_.reserve(count);
    std::array<AnimeInfoRawData, 512> batch{};
    auto& stream = fileInfo_.stream.value();
    for (size_t first = 0; first < count;) {
        const size_t entries = std::min(batch.size(), count - first);
        stream.read(reinterpret_cast<char*>(batch.data()), entries * sizeof(AnimeInfoRawData));
        for (size_t i = 0; i < entries; ++i) {
            const auto& entry = batch[i];
            cgl::AnimeResourceInfo info{};
            info.serialNum = {createInfo().version, entry.index};
            info.dataOffset = entry.address;
            info.motionCount = entry.motionCount;
            info.formatFlags = entry.unknown;
            map_[entry.index] = info; // Preserve last-entry-wins duplicate handling.
        }
        first += entries;
    }

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::query(
    const cgl::AnimeResourceSerialNum& serialNum,
    cgl::AnimeResourceInfo*            pInfo
)  {
    if (pInfo == nullptr) {
        LOGE("AnimeResourceInfo pointer is null");
        return cgl::Results::InvalidArgs;
    }

    if (serialNum.version != createInfo().version) {
        LOGE("Invalid version " << serialNum.version << " for query, "
             << "expected " << createInfo().version);
        return cgl::Results::InvalidArgs;
    }

    // find the entry
    auto it = map_.find(serialNum.value);
    if (it == map_.end()) {
        if (map_.empty()) {
            LOGE("Anime info map is empty, please load the reader first");
        } else {
            LOGE("Anime serial num " << serialNum.value << " not found");
        }
        return cgl::Results::IndexNotExist;
    }

    // fill the output info
    *pInfo = it->second;
    assert(pInfo->serialNum.version == createInfo().version);

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::queryAvailableSerialNums(
    std::vector<cgl::AnimeResourceSerialNum>* pList
) noexcept {
    if (pList == nullptr) {
        LOGE("AnimeResourceSerialNum list pointer is null");
        return cgl::Results::InvalidArgs;
    }

    pList->reserve(map_.size());
    for (const auto& [key, value] : map_) {
        pList->emplace_back(value.serialNum);
    }
    LOGD("Query available anime serial nums, count: " << pList->size());

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
// cgm::IAnimeInfoReader
// -----------------------------------------------------------------------------
IAnimeInfoReader::Ptr cgl::IAnimeInfoReader::create(
    const CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::COUNT)) {
        LOGE("Invalid create info for IAnimeInfoReader");
        return nullptr;
    }

    return std::make_unique<ReaderImpl>(createInfo);
}
