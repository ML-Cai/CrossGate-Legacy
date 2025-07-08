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
#include "cgl/resources/graphics_resource_file_info_reader.h"
#include "cgl/utils/enum_string_helper.h"
#include "utils/file_utils.h"
#include "utils/logger.h"

using cgl::IGraphicsResourceFileInfoReader;

// -----------------------------------------------------------------------------
// anonymouse namespace
// -----------------------------------------------------------------------------
namespace {

#pragma pack(1)
struct GraphicInfoEntry {
    int32_t graphicindex;
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
    int32_t mapIndex;
};
#pragma pack()


struct DataRange {
    int32_t min;
    int32_t max;
};


inline cgl::GraphicsResourceInfo retile(
    cgl::CrossGateVersion version,
    GraphicInfoEntry*     pCtx
) noexcept {
    cgl::GraphicsResourceInfo ret(
        version,
        cgl::GraphicsResourceIndex{
            cgl::GraphicsResourceIndexTypes::GraphicsBasedIndex,
            version,
            pCtx->graphicindex
        },
        cgl::GraphicsResourceIndex{
            cgl::GraphicsResourceIndexTypes::MapBasedIndex,
            version,
            pCtx->mapIndex
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

class GraphicIndexReaderImpl : public cgl::IGraphicsResourceFileInfoReader {
 public:
    explicit GraphicIndexReaderImpl(
        const cgl::IGraphicsResourceFileInfoReader::CreateInfo& createInfo);

    ~GraphicIndexReaderImpl();

    cgl::Results load() override;

    size_t infoCount() const noexcept override;

    bool mightContain(
        const cgl::GraphicsResourceIndex& mapIndex) const noexcept override;

    cgl::Results query(
        const cgl::GraphicsResourceIndex& index,
        cgl::GraphicsResourceInfo*        pGfxResInfo) const noexcept override;

 private:
    void destroy();

    bool preLoad(size_t bufferSize);

    DataRange mapIdxRange_;
    DataRange gfxIdxRange_;

    cgl::FileOpenInfo fileOpenInfo_;
    std::unique_ptr<uint8_t[]> pBuffer_;

    GraphicInfoEntryMap mapIdxMap_;
    GraphicInfoEntryMap graphicsIdxMap_;
};

}  // namespace

// -----------------------------------------------------------------------------
GraphicIndexReaderImpl::GraphicIndexReaderImpl(
    const cgl::IGraphicsResourceFileInfoReader::CreateInfo& createInfo)
    : cgl::IGraphicsResourceFileInfoReader(createInfo),
      mapIdxRange_(0, 0),
      gfxIdxRange_(0, 0) {
}

// -----------------------------------------------------------------------------
GraphicIndexReaderImpl::~GraphicIndexReaderImpl() {
    destroy();
}

// -----------------------------------------------------------------------------
void GraphicIndexReaderImpl::destroy() {
    if ((fileOpenInfo_.stream.has_value()) &&
        (fileOpenInfo_.stream->is_open())) {
        fileOpenInfo_.stream->close();
    }

    pBuffer_ = nullptr;
}

// -----------------------------------------------------------------------------
cgl::Results GraphicIndexReaderImpl::load() {
    // Get file resource path configurations.
    auto pSettings = createInfo().pSettings;
    auto resPath   = pSettings->crossGateResourcePath(createInfo().version);
    if ((resPath.version == cgl::CrossGateVersion::CG_VERSION_UNKNOWN) ||
        (resPath.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version {}",
             cgl::GetString(createInfo().version));
        return cgl::Results::Fail;
    }

    // prevent load multiple times.
    if (!graphicsIdxMap_.empty() || !mapIdxMap_.empty()) {
        LOGW("Skip load() since the reader had been loaded");
        return cgl::Results::Success;
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
        std::filesystem::path(resPath.graphicsInfoSubPath);
    LOGD("Load graphics index from file `{}`", fullPath.string());

    fileOpenInfo_ = cgl::TryOpenBinaryFile(fullPath);
    if (fileOpenInfo_.result != cgl::Results::Success) {
        LOGE("Failed to open graphic index file {}, msg {}",
             fullPath.string(), fileOpenInfo_.errorMsg);
        return fileOpenInfo_.result;
    }

    // verify the file size
    size_t bufferSize = GetFileSize(&fileOpenInfo_);
    if (bufferSize % sizeof(GraphicInfoEntry) != 0) {
        LOGE("Failed to read the data from unmatched/borken graphics resource"
             "info file, path : {}", fileOpenInfo_.path.string());
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
    auto& f = fileOpenInfo_.stream.value();
    f.read(reinterpret_cast<char *>(pBuffer_.get()), bufferSize);

    if (!f) {
        // it should not happened since we have check the file size before
        // preload(), but still check for safety.
        LOGE("File read failed: only {} bytes read (expected {})",
             f.gcount(), bufferSize);
        return false;
    }

    // save all entries
    const size_t count = bufferSize / sizeof(GraphicInfoEntry);
    auto pEntries = reinterpret_cast<GraphicInfoEntry *>(pBuffer_.get());

    for (size_t i = 0 ; i < count ; i++) {
        const auto& entry = pEntries[i];
        mapIdxRange_.min = std::min(mapIdxRange_.min, entry.mapIndex);
        mapIdxRange_.max = std::max(mapIdxRange_.max, entry.mapIndex);
        gfxIdxRange_.min = std::min(gfxIdxRange_.min, entry.graphicindex);
        gfxIdxRange_.max = std::max(gfxIdxRange_.max, entry.graphicindex);

        mapIdxMap_.emplace(entry.mapIndex, pEntries + i);
        graphicsIdxMap_.emplace(entry.graphicindex, pEntries + i);

        // debug purpose
        if (false) {
            LOGI("[{} / {}] width {}, height {}, offsetX {}, offsetY {},"
                "tileX {}, tileY {}, flag {}",
                pEntries[i].graphicindex,
                pEntries[i].mapIndex,
                pEntries[i].width,
                pEntries[i].height,
                pEntries[i].offsetX,
                pEntries[i].offsetY,
                pEntries[i].tileX,
                pEntries[i].tileY,
                static_cast<int>(pEntries[i].passableFlag));
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
bool GraphicIndexReaderImpl::mightContain(
    const cgl::GraphicsResourceIndex& idx
) const noexcept {
    switch (idx.type) {
    case cgl::GraphicsResourceIndexTypes::GraphicsBasedIndex:
        return (gfxIdxRange_.min <= idx.value) &&
               (idx.value <= gfxIdxRange_.max);

    case cgl::GraphicsResourceIndexTypes::MapBasedIndex:
        return (mapIdxRange_.min <= idx.value) &&
               (idx.value <= mapIdxRange_.max);

    default:
        assert(false && "Unknown GraphicsResourceIndexTypes value");
        return false;
    }
}

// -----------------------------------------------------------------------------
cgl::Results GraphicIndexReaderImpl::query(
    const cgl::GraphicsResourceIndex& idx,
    cgl::GraphicsResourceInfo*        pGfxResInfo
) const noexcept {
    if (this->mightContain(idx) == false) {
        return cgl::Results::IndexNotExist;
    }

    const GraphicInfoEntryMap *pEntryMap = nullptr;
    switch (idx.type) {
    case cgl::GraphicsResourceIndexTypes::GraphicsBasedIndex:
        pEntryMap = &graphicsIdxMap_;
        break;

    case cgl::GraphicsResourceIndexTypes::MapBasedIndex:
        pEntryMap = &mapIdxMap_;
        break;

    default:
        assert(false && "Unknown GraphicsResourceIndexTypes value");
        return cgl::Results::InvalidArgs;
    }

    // lookup the data
    auto iter = pEntryMap->find(idx.value);
    if (iter == pEntryMap->end()) {
        return cgl::Results::IndexNotExist;
    }

    // retile data to resource
    *pGfxResInfo = retile(createInfo().version, iter->second);

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
size_t GraphicIndexReaderImpl::infoCount() const noexcept {
    return graphicsIdxMap_.size();
}

// -----------------------------------------------------------------------------
// cgl::IGraphicsResourceFileInfoReader
// -----------------------------------------------------------------------------
cgl::IGraphicsResourceFileInfoReader::Ptr
cgl::IGraphicsResourceFileInfoReader::create(
    const cgl::IGraphicsResourceFileInfoReader::CreateInfo& createInfo
) {
    // check args
    if ((createInfo.pSettings == nullptr) ||
        (createInfo.version >= cgl::CrossGateVersion::Count)) {
        return nullptr;
    }

    return std::make_unique<GraphicIndexReaderImpl>(createInfo);
}
