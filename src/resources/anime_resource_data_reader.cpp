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
#include <sstream>
#include <iostream>
#include "cgl/settings/settings.h"
#include "cgl/resources/anime_resource_data_reader.h"
#include "cgl/utils/enum_string_helper.h"
#include "utils/file_utils.h"
#include "utils/logger.h"
#include "utils/common.h"

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


class ReaderImpl : public cgl::IAnimeResourceDataReader,
                          cgl::FileBlockCache {
 public:
    explicit ReaderImpl(
        const cgl::IAnimeResourceDataReader::CreateInfo& createInfo);

    ~ReaderImpl();

    cgl::Results load() override;

    cgl::Results query(
        const cgl::AnimeResourceInfo& animeResInfo,
        cgl::AnimeResourceData*       pAnimeResData) override;

    void destroy();

 private:
    cgl::CrossGateVersion version_;

    std::vector<uint8_t> tempBlock_;
};

}   // namespace

ReaderImpl::ReaderImpl(
    const cgl::IAnimeResourceDataReader::CreateInfo& createInfo)
    : cgl::IAnimeResourceDataReader(createInfo),
      cgl::FileBlockCache(4096, 1024) {
}

ReaderImpl::~ReaderImpl() {
    destroy();
}

void ReaderImpl::destroy() {
}

cgl::Results ReaderImpl::load() {
    // prevent load multiple times.
    if (isStreamReady()) {
        LOGW("The anime resource data reader has already opened a file");
        return cgl::Results::Success;
    }

    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPath   = pSettings->crossGateResourcePath(createInfo().version);
    if ((resPath.version == cgl::CrossGateVersion::CG_VERSION_UNKNOWN) ||
        (resPath.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version {}",
             cgl::GetString(createInfo().version));
        return cgl::Results::InvalidFile;
    }

    // open the file
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->crossGateResourceRootDir) /
        std::filesystem::path(resPath.animeDataSubPath);
    LOGD("Load graphics data from file `{}`", fullPath.string());

    auto result = this->open(fullPath);
    if (result != cgl::Results::Success) {
        LOGE("Failed to open graphic date file {}", fullPath.string());
        return result;
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
             "reader was created for `{}`, but the query requested data from "
             "`{}`",
             cgl::GetString(createInfo().version),
             cgl::GetString(animeResInfo.serialNum.version));
        return cgl::Results::InvalidArgs;
    }

    // Verify the index range, Every motion is require at least
    // 1 AnimeDataRawMotionHeader + 1 AnimeRawFrameData for the data reading.
    size_t offset = animeResInfo.dataOffset;
    size_t readBound = offset +
                       sizeof(cgl::AnimeResourceInfo) +
                       sizeof(cgl::AnimeMotionDesc) * animeResInfo.motionCount;
    if (readBound > this->fileSize()) {
        LOGE("AnimeIndex {}'s address {} is out of range {}.",
            animeResInfo.serialNum.value, animeResInfo.dataOffset, fileSize());
        return cgl::Results::InvalidArgs;
    }

    // read all motions
    pAnimeResData->serialNum = animeResInfo.serialNum;
    pAnimeResData->motionMap.clear();
    pAnimeResData->motionMap.reserve(animeResInfo.motionCount);
    cgl::AnimeMotionDesc motionDesc;

    for (uint16_t mIdx = 0; mIdx < animeResInfo.motionCount; mIdx++) {
        // read motion header
        AnimeDataRawMotionHeader header;
        if (read(offset, sizeof(header), &header) != cgl::Results::Success) {
            LOGE("Failed to read motion header for AnimeIndex {} at offset {}",
                 animeResInfo.serialNum.value, offset);
            return cgl::Results::Fail;
        }
        offset += sizeof(header);   // offset to read motion data

        // verify the motion header
        if (header.direction >= static_cast<int>(cgl::DirectionTypes::COUNT)) {
            LOGE("AnimeIndex {}'s direction {} is out of range.",
                 animeResInfo.serialNum.value, header.direction);
            continue;
        }
        if (header.motion >= static_cast<int>(cgl::MotionTypes::COUNT)) {
            LOGE("AnimeIndex {}'s motion {} is out of range.",
                 animeResInfo.serialNum.value, header.motion);
            continue;
        }

        // Read all motions data to temp buffer, assume the 1024 buffer is large
        // enough to hold all motion frames.
        static AnimeDataRawMotionFrame tempBuffer[1024];
        assert(header.frameCount <= 1024);
        size_t readCount = sizeof(AnimeDataRawMotionFrame) * header.frameCount;

        if (read(offset, readCount, tempBuffer) != cgl::Results::Success) {
            LOGE("Failed to read motion data for AnimeIndex {} at offset {}",
                 animeResInfo.serialNum.value, offset);
            return cgl::Results::Fail;
        }
        offset += readCount;   // offset to read next motion header


        // keep for debug purpose, dump the motion frames
        std::stringstream ss;
        for (uint32_t i = 0; i < header.frameCount; i++) {
            ss << tempBuffer[i].serialNum << " ";
        }
        LOGD("motion index [{}], header.direction {}, motion {} duraction {} "
             "frameCount {} , frames : {}",
            mIdx,
            header.direction,
            header.motion,
            header.duraction,
            header.frameCount,
            ss.str());


        // Update description
        auto pRawFrames = tempBuffer;
        cgl::AnimeMotionDesc desc {
            .direction = static_cast<cgl::DirectionTypes>(header.direction),
            .motion    = static_cast<cgl::MotionTypes>(header.motion),
            .duration  = header.duraction
        };
        desc.motionGraphicsSerialNums.reserve(header.frameCount);

        for (uint32_t i = 0; i < header.frameCount; i++) {
            desc.motionGraphicsSerialNums.emplace_back(pRawFrames[i].serialNum);
        }

        auto key = std::make_pair(desc.direction, desc.motion);
        pAnimeResData->motionMap.emplace(std::move(key), std::move(desc));
    }

    // Store the anime data
    // *pAnimeResData = std::move(animeData);
    // LOGI("AnimeResourceData for serialNum {} has {} motions",
    //      cgl::GetString(pAnimeResData->serialNum),
    //      pAnimeResData->motionMap.size());
    // LOGI("AnimeResourceData for serialNum {} has {} motions",
    //      cgl::GetString(pAnimeResData->serialNum),
    //      pAnimeResData->motionMap.size());
    return cgl::Results::Success;
}

//------------------------------------------------------------------------------
// cgl::IAnimeResourceDataReader
//------------------------------------------------------------------------------
cgl::IAnimeResourceDataReader::Ptr
cgl::IAnimeResourceDataReader::create(
    const cgl::IAnimeResourceDataReader::CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::Count)) {
        return nullptr;
    }

    return std::make_unique<ReaderImpl>(createInfo);
}
