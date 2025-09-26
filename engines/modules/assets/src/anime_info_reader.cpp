// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <fstream>
#include <filesystem>
#include <memory>
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

 private:
    void destroy();

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
    destroy();
}

// -----------------------------------------------------------------------------
void ReaderImpl::destroy() {
    if ((fileInfo_.stream.has_value()) &&
        (fileInfo_.stream->is_open())) {
        fileInfo_.stream->close();
    }
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::load() {
    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPaths  = cgl::AcquireCrossGateResourcePath(createInfo().version);
    if ((resPaths.version == cgl::CrossGateVersion::UNKNOWN) ||
        (resPaths.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version `"
              << createInfo().version << "`");
        return cgl::Results::Fail;
    }

    // close previous data first.
    if (fileInfo_.isOpen() == true) {
        LOGW("The graphic index reader has already opened a file. Release the"
             "previous one ...");
        destroy();
    }

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
    if (fileSize % sizeof(AnimeInfoRawData) != 0) {
        LOGE("Failed to read the data from unmatch anime info file");
        destroy();
        return cgl::Results::InvalidFile;
    }

    // Anime info file is not sorted and just few kilobytes in size (usually
    // less than 10KB) in most cases, so we can preload all entries to memory
    // in this case. Maybe we can use a sorted file in the future or something
    // like that, then we can use binary search to query the data.
    // But for now, we just read all data to memory.
    return preLoad(fileSize);
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::preLoad(size_t fileSize) {
    // allocate buffers
    auto pBuffer = std::make_unique<uint8_t[]>(fileSize);
    assert(pBuffer != nullptr);

    // read all buffer data
    auto& f = fileInfo_.stream.value();
    f.read(reinterpret_cast<char *>(pBuffer.get()), fileSize);

    if (!f) {
        // it should not happened since we have check the file size before
        // preload(), but still check for safety.
        LOGE("File read failed: only " << f.gcount() <<" bytes readed "
             << "(expected {" << fileSize << "})");
        return cgl::Results::InvalidFile;
    }

    // parse all entries
    const size_t count = fileSize / sizeof(AnimeInfoRawData);
    auto pEntries = reinterpret_cast<AnimeInfoRawData *>(pBuffer.get());
    map_.reserve(count);

    for (size_t i = 0; i < count; i++) {
        const auto& entry = pEntries[i];

        // // debug purpose
        // if (true) {
        //     LOGI("- i[{}] index {}, address {}, motionCount {}",
        //          i, entry.index, entry.address, entry.motionCount);
        // }

        // fill the map
        cgl::AnimeResourceInfo info;
        info.serialNum.version = createInfo().version;
        info.serialNum.value   = entry.index;
        info.dataOffset        = entry.address;
        info.motionCount       = entry.motionCount;

        map_[entry.index] = std::move(info);
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
