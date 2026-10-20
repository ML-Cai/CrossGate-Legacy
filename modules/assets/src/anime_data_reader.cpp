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
#include "cgl/assets/anime_data_reader.h"
#include "cgl/settings/settings.h"
#include "cgl/utils/filesystem.h"
#include "cgl/trace/logger.h"
#include "assets_path_info.h"

// -----------------------------------------------------------------------------
// anonymous namespace
// -----------------------------------------------------------------------------
namespace {

#pragma pack(1)
struct AnimeDataRawMotionHeader {
    uint16_t direction;
    uint16_t motion;
    uint32_t duraction;
    uint32_t frameCount;
};
#pragma pack()


#pragma pack(1)
struct AnimeDataRawMotionFrame {
    uint32_t serialNum;
    uint8_t unknown[6];
};
#pragma pack()


class ReaderImpl : public cgl::IAnimeDataReader,
                          cgl::FileBlockCache {
 public:
    explicit ReaderImpl(
        const cgl::IAnimeDataReader::CreateInfo& createInfo);

    ~ReaderImpl();

    cgl::Results load() override;

    cgl::Results query(
        const cgl::AnimeResourceInfo& animeResInfo,
        cgl::AnimeResourceData*       pAnimeResData) override;

    void close() noexcept override;

};

}   // namespace

ReaderImpl::ReaderImpl(
    const cgl::IAnimeDataReader::CreateInfo& createInfo)
    : cgl::IAnimeDataReader(createInfo),
      cgl::FileBlockCache(4096, 1024) {
}

ReaderImpl::~ReaderImpl() {
    close();
}

void ReaderImpl::close() noexcept {
    cgl::FileBlockCache::close();
}

cgl::Results ReaderImpl::load() {
    // prevent load multiple times.
    if (isStreamReady()) {
        LOGW("The anime resource data reader has already opened a file");
        return cgl::Results::Success;
    }

    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPaths  = cgl::AcquireCrossGateResourcePath(createInfo().version);
    if ((resPaths.version == cgl::CrossGateVersion::UNKNOWN) ||
        (resPaths.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version `"
             << createInfo().version << "`");
        return cgl::Results::Fail;
    }

    // open the file
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->general.EngineRootPath) /
        std::filesystem::path(resPaths.animeDataSubPath);
    LOGD("Load graphics index from file `" << fullPath.string() << "`");

    if (this->open(fullPath) == false) {
        LOGE("Failed to open graphic date file :" << fullPath.string());
        return cgl::Results::InvalidFile;
    }

    return cgl::Results::Success;
}

cgl::Results ReaderImpl::query(
    const cgl::AnimeResourceInfo& animeResInfo,
    cgl::AnimeResourceData*       pAnimeResData
) {
    // Check args
    if (pAnimeResData == nullptr) {
        LOGE("AnimeResourceData pointer is null");
        return cgl::Results::InvalidArgs;
    }

    // keep for debug purpose
    // LOGD("Query anime data for serial num {}:{}, offset {}",
    //      cgl::GetString(animeResInfo.serialNum.version),
    //      animeResInfo.serialNum.value,
    //      animeResInfo.dataOffset);

    if (animeResInfo.serialNum.version != createInfo().version) {
        LOGE("Attempted to read anime data from a mismatched version: the "
             "reader was created for `" << createInfo().version
             << "`, but the query requested data from `"
             << animeResInfo.serialNum.version << "`");
        return cgl::Results::InvalidArgs;
    }

    // Validate on-disk sizes using subtraction, before allocating or changing output.
    size_t offset = animeResInfo.dataOffset;
    if (!isStreamReady() || offset > fileSize() ||
        animeResInfo.motionCount > (fileSize() - offset) / sizeof(AnimeDataRawMotionHeader)) {
        return cgl::Results::InvalidFile;
    }
    constexpr size_t maxFramesPerMotion = 65536;
    constexpr size_t maxFramesPerQuery = 1024 * 1024;
    size_t totalFrames = 0;
    cgl::AnimeResourceData result{};
    result.serialNum = animeResInfo.serialNum;
    result.motionMap.reserve(animeResInfo.motionCount);
    std::vector<AnimeDataRawMotionFrame> frames;
    for (uint16_t mIdx = 0; mIdx < animeResInfo.motionCount; ++mIdx) {
        AnimeDataRawMotionHeader header{};
        if (!read(offset, sizeof(header), &header)) return cgl::Results::InvalidFile;
        offset += sizeof(header);
        if (header.frameCount > maxFramesPerMotion ||
            header.frameCount > maxFramesPerQuery - totalFrames ||
            header.frameCount > (fileSize() - offset) / sizeof(AnimeDataRawMotionFrame)) {
            return cgl::Results::InvalidFile;
        }
        totalFrames += header.frameCount;
        const size_t bytes = size_t(header.frameCount) * sizeof(AnimeDataRawMotionFrame);
        // Unknown motions are skipped, including their frame payload.
        if (header.direction >= static_cast<int>(cgl::DirectionTypes::COUNT) ||
            header.motion >= static_cast<int>(cgl::MotionTypes::COUNT)) {
            offset += bytes;
            continue;
        }
        frames.resize(header.frameCount);
        if (bytes && !read(offset, bytes, frames.data())) return cgl::Results::InvalidFile;
        offset += bytes;
        cgl::AnimeMotionDesc desc{
            .version = createInfo().version,
            .direction = static_cast<cgl::DirectionTypes>(header.direction),
            .motion = static_cast<cgl::MotionTypes>(header.motion),
            .duration = header.duraction
        };
        desc.motionGraphicsSerialNums.reserve(header.frameCount);
        for (const auto& frame : frames) desc.motionGraphicsSerialNums.push_back(frame.serialNum);
        auto key = std::make_pair(desc.direction, desc.motion);
        result.motionMap.emplace(key, std::move(desc));
    }
    *pAnimeResData = std::move(result);
    return cgl::Results::Success;
}

//------------------------------------------------------------------------------
// cgl::IAnimeDataReader
//------------------------------------------------------------------------------
cgl::IAnimeDataReader::Ptr
cgl::IAnimeDataReader::create(
    const cgl::IAnimeDataReader::CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::COUNT)) {
        return nullptr;
    }

    return std::make_unique<ReaderImpl>(createInfo);
}
