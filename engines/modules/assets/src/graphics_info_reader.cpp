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
#include "cgl/assets/graphics_info_reader.h"
#include "cgl/settings/settings.h"
#include "cgl/utils/filesystem.h"
#include "cgl/trace/logger.h"
#include "assets_path_info.h"

using cgl::IGraphicsInfoReader;

// -----------------------------------------------------------------------------
// anonymouse namespace
// -----------------------------------------------------------------------------
namespace {

#pragma pack(1)
struct GraphicInfoEntry {
    uint32_t graphicSerialNum;
    uint32_t dataOffset;
    uint32_t dataSize;
    int32_t offsetX;
    int32_t offsetY;
    int32_t width;
    int32_t height;
    uint8_t tileX;
    uint8_t tileY;
    uint8_t passableFlag;
    uint8_t unknown[5];
    uint32_t mapSerialNum;
};
#pragma pack()


struct DataRange {
    uint32_t min;
    uint32_t max;
};


inline cgl::GraphicsResourceInfo retile(
    cgl::CrossGateVersion version,
    GraphicInfoEntry*     pCtx
) noexcept {
    cgl::GraphicsResourceInfo ret(
        version,
        cgl::GraphicsResourceSerialNum{
            cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum,
            version,
            pCtx->graphicSerialNum
        },
        cgl::GraphicsResourceSerialNum{
            cgl::GraphicsResourceSerialNumTypes::MapSerialNum,
            version,
            pCtx->mapSerialNum
        },
        pCtx->dataOffset,
        pCtx->dataSize,
        pCtx->offsetX,
        pCtx->offsetY,
        pCtx->width,
        pCtx->height);

    return ret;
}

}   // namespace

// -----------------------------------------------------------------------------
// cgl::GraphicIndexReader
// -----------------------------------------------------------------------------
namespace {

using GraphicInfoEntryMap = std::unordered_map<int32_t, GraphicInfoEntry*>;

class GraphicIndexReaderImpl : public cgl::IGraphicsInfoReader {
 public:
    explicit GraphicIndexReaderImpl(
        const cgl::IGraphicsInfoReader::CreateInfo& createInfo);

    ~GraphicIndexReaderImpl();

    cgl::Results load() override;

    size_t infoCount() const noexcept override;

    bool mightContain(const cgl::GraphicsResourceSerialNum& mapSerialNum)
        const noexcept override;

    cgl::Results query(const cgl::GraphicsResourceSerialNum& serialNum,
                       cgl::GraphicsResourceInfo*            pGfxResInfo)
        const noexcept override;

    cgl::Results queryAvailableSerialNums(
        std::vector<int32_t>* pList) noexcept override;

 private:
    void destroy();

    bool preLoad(size_t bufferSize);

    DataRange mapIdxRange_;
    DataRange gfxIdxRange_;

    cgl::FileInfo fileInfo_;
    std::unique_ptr<uint8_t[]> pBuffer_;

    GraphicInfoEntryMap mapIdxMap_;
    GraphicInfoEntryMap graphicsIdxMap_;
};

}  // namespace

// -----------------------------------------------------------------------------
GraphicIndexReaderImpl::GraphicIndexReaderImpl(
    const cgl::IGraphicsInfoReader::CreateInfo& createInfo)
    : cgl::IGraphicsInfoReader(createInfo),
      mapIdxRange_(0, 0),
      gfxIdxRange_(0, 0) {
}

// -----------------------------------------------------------------------------
GraphicIndexReaderImpl::~GraphicIndexReaderImpl() {
    destroy();
}

// -----------------------------------------------------------------------------
void GraphicIndexReaderImpl::destroy() {
    if ((fileInfo_.stream.has_value()) &&
        (fileInfo_.stream->is_open())) {
        fileInfo_.stream->close();
    }

    pBuffer_ = nullptr;
}

// -----------------------------------------------------------------------------
cgl::Results GraphicIndexReaderImpl::load() {
    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPaths  = cgl::AcquireCrossGateResourcePath(createInfo().version);
    if ((resPaths.version == cgl::CrossGateVersion::UNKNOWN) ||
        (resPaths.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version `"
              << cgl::ToStr(createInfo().version) << "`");
        return cgl::Results::Fail;
    }

    // prevent load multiple times.
    if (!graphicsIdxMap_.empty() || !mapIdxMap_.empty()) {
        LOGW("Skip load() since the reader had been loaded");
        return cgl::Results::Success;
    }

    // close previous data first.
    if (fileInfo_.isOpen() == true) {
        LOGW("The graphic index reader has already opened a file. Release the"
             "previous one ...");
        destroy();
    }

    // // load file
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->general.EngineRootPath) /
        std::filesystem::path(resPaths.graphicsInfoSubPath);
    LOGD("Load graphics index from file `" << fullPath.string() << "`");

    fileInfo_ = cgl::TryOpenBinaryFile(fullPath);
    if (fileInfo_.isOpen() == false) {
        LOGE("Failed to open graphic index file " << fullPath.string()
             << ", msg " << fileInfo_.errorMsg);
        return cgl::Results::InvalidFile;
    }

    // verify the file size
    size_t bufferSize = GetFileSize(&fileInfo_);
    if (bufferSize % sizeof(GraphicInfoEntry) != 0) {
        LOGE("Failed to read the data from unmatched/borken graphics resource"
             "info file, path : " << fileInfo_.path.string());
        destroy();
        return cgl::Results::InvalidFile;
    }

    // preload all info entries
    preLoad(bufferSize);

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
bool GraphicIndexReaderImpl::preLoad(size_t bufferSize) {
    // allocate buffers
    pBuffer_ = std::make_unique<uint8_t[]>(bufferSize);
    assert(pBuffer_ != nullptr);

    // read all buffer data
    auto& f = fileInfo_.stream.value();
    f.read(reinterpret_cast<char *>(pBuffer_.get()), bufferSize);

    if (!f) {
        // it should not happened since we have check the file size before
        // preload(), but still check for safety.
        LOGE("File read failed: only " << f.gcount() <<" bytes readed "
             << "(expected {" << bufferSize << "})");
        return false;
    }

    // save all entries
    const size_t count = bufferSize / sizeof(GraphicInfoEntry);
    auto pEntries = reinterpret_cast<GraphicInfoEntry *>(pBuffer_.get());

    for (size_t i = 0 ; i < count ; i++) {
        const auto& entry = pEntries[i];
        mapIdxRange_.min = std::min(mapIdxRange_.min, entry.mapSerialNum);
        mapIdxRange_.max = std::max(mapIdxRange_.max, entry.mapSerialNum);
        gfxIdxRange_.min = std::min(gfxIdxRange_.min, entry.graphicSerialNum);
        gfxIdxRange_.max = std::max(gfxIdxRange_.max, entry.graphicSerialNum);

        mapIdxMap_.emplace(entry.mapSerialNum, pEntries + i);
        graphicsIdxMap_.emplace(entry.graphicSerialNum, pEntries + i);

        // debug purpose
        if (false) {
            LOGI("[" << pEntries[i].graphicSerialNum
                << " / " << pEntries[i].mapSerialNum
                << "], width " << pEntries[i].width
                << ", height " << pEntries[i].height
                << ", offsetX " << pEntries[i].offsetX
                << ", offsetY " << pEntries[i].offsetY
                << ", tileX " << pEntries[i].tileX
                << ", tileY " << pEntries[i].tileY
                << ", flag " << static_cast<int>(pEntries[i].passableFlag)
               );
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
bool GraphicIndexReaderImpl::mightContain(
    const cgl::GraphicsResourceSerialNum& serialNum
) const noexcept {
    switch (serialNum.type) {
    case cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum:
        return (gfxIdxRange_.min <= serialNum.value) &&
               (serialNum.value <= gfxIdxRange_.max);

    case cgl::GraphicsResourceSerialNumTypes::MapSerialNum:
        return (mapIdxRange_.min <= serialNum.value) &&
               (serialNum.value <= mapIdxRange_.max);

    default:
        assert(false && "Unknown GraphicsResourceSerialNumTypes value");
        return false;
    }
}

// -----------------------------------------------------------------------------
cgl::Results GraphicIndexReaderImpl::query(
    const cgl::GraphicsResourceSerialNum& serialNum,
    cgl::GraphicsResourceInfo*            pGfxResInfo
) const noexcept {
    if (this->mightContain(serialNum) == false) {
        return cgl::Results::IndexNotExist;
    }

    const GraphicInfoEntryMap *pEntryMap = nullptr;
    switch (serialNum.type) {
    case cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum:
        pEntryMap = &graphicsIdxMap_;
        break;

    case cgl::GraphicsResourceSerialNumTypes::MapSerialNum:
        pEntryMap = &mapIdxMap_;
        break;

    default:
        assert(false && "Unknown GraphicsResourceSerialNumTypes value");
        return cgl::Results::InvalidArgs;
    }

    // lookup the data
    auto iter = pEntryMap->find(serialNum.value);
    if (iter == pEntryMap->end()) {
        return cgl::Results::IndexNotExist;
    }

    // retile data to resource
    *pGfxResInfo = retile(createInfo().version, iter->second);

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
cgl::Results GraphicIndexReaderImpl::queryAvailableSerialNums(
    std::vector<int32_t>* pList
) noexcept {
    if (pList == nullptr) {
        return cgl::Results::InvalidArgs;
    }

    pList->reserve(graphicsIdxMap_.size());
    for (const auto& [key, value] : graphicsIdxMap_) {
        pList->emplace_back(value->graphicSerialNum);
    }
    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
size_t GraphicIndexReaderImpl::infoCount() const noexcept {
    return graphicsIdxMap_.size();
}

// -----------------------------------------------------------------------------
// cgl::IGraphicsInfoReader
// -----------------------------------------------------------------------------
cgl::IGraphicsInfoReader::Ptr
cgl::IGraphicsInfoReader::create(
    const cgl::IGraphicsInfoReader::CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::COUNT)) {
        return nullptr;
    }

    return std::make_unique<GraphicIndexReaderImpl>(createInfo);
}
