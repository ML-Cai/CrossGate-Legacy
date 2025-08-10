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
#include "cgl/settings/settings.h"
#include "cgl/resources/anime_resource_info_reader.h"
#include "cgl/utils/enum_string_helper.h"
#include "utils/file_utils.h"
#include "utils/logger.h"

using cgl::IAnimeResourceInfoReader;

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


class ReaderImpl : public cgl::IAnimeResourceInfoReader {
 public:
    explicit ReaderImpl(
        const cgl::IAnimeResourceInfoReader::CreateInfo& createInf);

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

    cgl::FileOpenInfo fileOpenInfo_;

    std::unordered_map<uint32_t, cgl::AnimeResourceInfo> map_;
};

}   // namespace


// -----------------------------------------------------------------------------
// AnimeInfoRawData Impl
// -----------------------------------------------------------------------------
ReaderImpl::ReaderImpl(
    const cgl::IAnimeResourceInfoReader::CreateInfo& createInfo)
    : cgl::IAnimeResourceInfoReader(createInfo) {
}

// -----------------------------------------------------------------------------
ReaderImpl::~ReaderImpl() {
    destroy();
}

// -----------------------------------------------------------------------------
void ReaderImpl::destroy() {
    if ((fileOpenInfo_.stream.has_value()) &&
        (fileOpenInfo_.stream->is_open())) {
        fileOpenInfo_.stream->close();
    }
}

// -----------------------------------------------------------------------------
cgl::Results ReaderImpl::load() {
    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPath   = pSettings->crossGateResourcePath(createInfo().version);
    if ((resPath.version == cgl::CrossGateVersion::CG_VERSION_UNKNOWN) ||
        (resPath.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version {}",
             cgl::GetString(createInfo().version));
        return cgl::Results::Fail;
    }

    // close previous data first.
    if (cgl::IsFileOpen(fileOpenInfo_)) {
        LOGW("The graphic index reader has already opened a file. Release the"
             "previous one ...");
        destroy();
    }

    // load file
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->crossGateResourceRootDir) /
        std::filesystem::path(resPath.animeIndexSubPath);
    LOGD("Load graphics index from file `{}`", fullPath.string());

    fileOpenInfo_ = cgl::TryOpenBinaryFile(fullPath);
    if (fileOpenInfo_.result != cgl::Results::Success) {
        LOGE("Failed to open graphic index file {}, msg {}",
             fullPath.string(), fileOpenInfo_.errorMsg);
        return fileOpenInfo_.result;
    }

    // verify the size
    size_t fileSize = GetFileSize(&fileOpenInfo_);
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
    auto& f = fileOpenInfo_.stream.value();
    f.read(reinterpret_cast<char *>(pBuffer.get()), fileSize);

    if (!f) {
        // it should not happened since we have check the file size before
        // preload(), but still check for safety.
        LOGE("File read failed: only {} bytes read (expected {})",
             f.gcount(), fileSize);
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
        LOGE("Invalid version {} for query, expected {}",
             cgl::GetString(serialNum.version),
             cgl::GetString(createInfo().version));
        return cgl::Results::InvalidArgs;
    }

    // find the entry
    auto it = map_.find(serialNum.value);
    if (it == map_.end()) {
        if (map_.empty()) {
            LOGE("Anime info map is empty, please load the reader first");
        } else {
            LOGE("Anime info with serial num {} not found", serialNum.value);
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
    LOGD("Query available anime serial nums, count: {}", pList->size());

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
// cgm::IAnimeResourceInfoReader
// -----------------------------------------------------------------------------
IAnimeResourceInfoReader::Ptr cgl::IAnimeResourceInfoReader::create(
    const CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::Count)) {
        LOGE("Invalid create info for IAnimeResourceInfoReader");
        return nullptr;
    }

    return std::make_unique<ReaderImpl>(createInfo);
}
