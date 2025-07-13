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
#include "cgl/settings/settings.h"
#include "cgl/resources/graphics_resource_file_data_reader.h"
#include "cgl/utils/enum_string_helper.h"
#include "utils/file_utils.h"
#include "utils/logger.h"

//------------------------------------------------------------------------------
// namespace anonymous
//------------------------------------------------------------------------------
namespace {

enum GraphicsDataFlags : uint8_t {
    DecompressedData  = 0x0,
    CompressedData    = 0x1,
    DataVerPUK2       = 0x2,
};

// Basic graphic data header, shared with PUK1, PUK2 and later version.
#pragma pack(1)
struct GraphicsFileDataHeader {
    // Assume as a check code for graphic data header.
    // Fixed data present as code[0]: R, code [1]: D.
    char code[2];

    // Graphics data flags.
    GraphicsDataFlags flag;

    // Unknown data.
    uint8_t unknown;

    // Width of graphic data.
    int32_t width;

    // Height of graphic data.
    int32_t height;

    // Size of graphic data block.
    // This size is involve the Header(RawGraphicsFileDataHeader) and the Data
    // block.
    uint32_t dataSize;
};
#pragma pack()

#pragma pack(1)
struct GraphicsFileDataHeader_PUK2 {
    // PUK2 palette data, PUK1 or older version doesn't have this elements
    uint32_t paletteSize;
};
#pragma pack()

std::string toString(const GraphicsFileDataHeader &h) {
    std::stringstream ss;
    ss << "code[0] " << h.code[0] << "(" << static_cast<int>(h.code[0]) << ")"
       << ", code[1] " << h.code[1] << "(" << static_cast<int>(h.code[1]) << ")"
       << ", unknown " << h.unknown
       << ", width " << h.width
       << ", height " << h.height
       << ", dataSize " << h.dataSize
       << std::endl;

    return ss.str();
}

//
// Decompress algorithm : https://cgsword.com/filesystem_graphicmap.htm
//
bool Decompress(uint8_t*  pDataBlock,
                size_t    dataBlockSize,
                uint8_t*  pGraphicFileData,
                size_t    graphicFileDataSize,
                char*     pErrorMsgBuf,
                size_t    errorMsgBufSize) {
    constexpr size_t HEX_100 = 0x100;    // 0x100
    constexpr size_t HEX_10000 = 0x10000;    // 0x10000
    uint8_t* p = pGraphicFileData;

    for (size_t i = 0 ; i < dataBlockSize ; i++) {
        const uint8_t code = pDataBlock[i] >> 4;
        const uint8_t a = pDataBlock[i] & 0x0F;
        const size_t remainSize = (graphicFileDataSize -
                                   static_cast<size_t>(p - pGraphicFileData));
        // LOGI("code {:X}", pDataBlock[i]);

        if (code == 0x0) {
            // 0a | xx :
            //  Read HEX(a) count of XX
            const size_t count = std::min(remainSize, static_cast<size_t>(a));
            memcpy(p, &pDataBlock[i + 1], count);
            p += count;
            i += count;
        } else if (code == 0x1) {
            // 1a | bb | xx :
            //  Read HEX(a * 100 + bb) count of xx
            const uint8_t bb = pDataBlock[i + 1];
            const size_t count = std::min(remainSize, a * HEX_100 + bb);
            memcpy(p, &pDataBlock[i + 2], count);
            p += count;
            i += 1 + count;
        } else if (code == 0x2) {
            // 2a | bb | cc | xx :
            //  Read HEX(a * 10000 + bb * 100 + cc) count of xx
            const uint8_t bb = pDataBlock[i + 1];
            const uint8_t cc = pDataBlock[i + 2];
            const size_t count = std::min(remainSize,
                                          a * HEX_10000 + bb * HEX_100 + cc);
            memcpy(p, &pDataBlock[i + 3], count);
            p += count;
            i += 2 + count;
        } else if (code == 0x8) {
            // 8a | xx :
            //  Repeat HEX(a) count of transparent
            const uint8_t xx = pDataBlock[i + 1];
            const size_t count = std::min(remainSize, static_cast<size_t>(a));
            memset(p, xx, count);
            p += count;
            i += 1;
        } else if (code == 0x9) {
            // 9a | xx | bb :
            //  Repeat HEX(a * 100 + bb) count of xx
            const uint8_t xx = pDataBlock[i + 1];
            const uint8_t bb = pDataBlock[i + 2];
            const size_t count = std::min(remainSize, a * HEX_100 + bb);
            memset(p, xx, count);
            p += count;
            i += 2;
        } else if (code == 0xA) {
            // Aa | xx | bb | cc :
            //  Repeat HEX(a * 10000 + bb * 100 + cc) count of xx
            const uint8_t xx = pDataBlock[i + 1];
            const uint8_t bb = pDataBlock[i + 2];
            const uint8_t cc = pDataBlock[i + 3];
            const size_t count = std::min(remainSize,
                                          a * HEX_10000 + bb * HEX_100 + cc);
            memset(p, xx, count);
            p += count;
            i += 3;
        } else if (code == 0xC) {
            // Ca :
            //  Repeat HEX(a) count of transparent
            size_t count = std::min(remainSize, static_cast<size_t>(a));
            memset(p, 0, count);
            p += count;
        } else if (code == 0xD) {
            // Da | bb :
            //  Repeat HEX(a * 100 + bb) count of transparent
            const uint8_t bb = pDataBlock[i + 1];
            const size_t count = std::min(remainSize, a * HEX_100 + bb);
            memset(p, 0, count);
            p += count;
            i += 1;
        } else if (code == 0xE) {
            // Ea | bb | cc :
            //   Read HEX(a * 10000 + bb * 100 + cc) count of transparent
            const uint8_t bb = pDataBlock[i + 1];
            const uint8_t cc = pDataBlock[i + 2];
            const size_t count = std::min(remainSize,
                                          a * HEX_10000 + bb * HEX_100 + cc);
            memset(p, 0, count);
            p += count;
            i += 2;
        } else {
            sprintf_s(pErrorMsgBuf, errorMsgBufSize,
                      "Unknown header code %d", code);
            return false;
        }
        // LOGI("proc size {} , remain {}",
        //     (size_t)(p - pDataBlock), dataBlockSize - i);
    }

    // LOGI("headerSize {}", headerSize);
    // LOGI("Read size >>> {}", (size_t)(p - GraphicsFileData));
    assert(static_cast<size_t>(graphicFileDataSize) ==
           static_cast<size_t>(p - pGraphicFileData));

    return true;
}


}   // namespace

//------------------------------------------------------------------------------
// GraphicsDataReaderImpl
//------------------------------------------------------------------------------
namespace {

class GraphicsDataReaderImpl : public cgl::IGraphicsResourceFileDataReader,
                                      cgl::FileBlockCache {
 public:
    explicit GraphicsDataReaderImpl(
        const cgl::IGraphicsResourceFileDataReader::CreateInfo& createInfo);

    ~GraphicsDataReaderImpl();

    // Try to open specific Graphic*_*.bin data but not read data yet.
    cgl::Results load() override;

    cgl::Results query(const cgl::GraphicsResourceInfo& gfxResInfo,
                       cgl::GraphicsResourceData*       pGfxResData) override;

    void destroy();

 private:
    cgl::CrossGateVersion version_;

    cgl::Results ReadDataFromPUK(
        const cgl::GraphicsResourceInfo& gfxResInfo,
        const GraphicsFileDataHeader&    header,
        cgl::GraphicsResourceData*       pGfxFileData);

    std::vector<uint8_t> tempBlock_;
};

}   // namespace

GraphicsDataReaderImpl::GraphicsDataReaderImpl(
    const cgl::IGraphicsResourceFileDataReader::CreateInfo& createInfo)
    : cgl::IGraphicsResourceFileDataReader(createInfo),
      cgl::FileBlockCache(4096, 1024) {
}

GraphicsDataReaderImpl::~GraphicsDataReaderImpl() {
    destroy();
}

void GraphicsDataReaderImpl::destroy() {
}

cgl::Results GraphicsDataReaderImpl::load() {
    // prevent load multiple times.
    if (isStreamReady()) {
        LOGW("The graphic resource file data reader has already opened a file");
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

    // setup exceptions to capture the fail reasons.
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->crossGateResourceRootDir) /
        std::filesystem::path(resPath.graphicsDataSubPath);
    LOGD("Load graphics data from file `{}`", fullPath.string());

    auto result = this->open(fullPath);
    if (result != cgl::Results::Success) {
        LOGE("Failed to open graphic index file {}", fullPath.string());
        return result;
    }

    // verify the first two byte is "RD" or not
    char buffer[2] = {0};
    if (this->fileSize() > 2) {
        this->read(0, 2, reinterpret_cast<uint8_t*>(buffer));
    }
    if ((buffer[0] != 'R') || (buffer[1] != 'D')) {
        LOGE("Failed to read the data from unmatch graphics data file");
        destroy();
        return cgl::Results::InvalidFile;
    }

    return cgl::Results::Success;
}

cgl::Results GraphicsDataReaderImpl::ReadDataFromPUK(
    const cgl::GraphicsResourceInfo& gfxResInfo,
    const GraphicsFileDataHeader&    header,
    cgl::GraphicsResourceData*       pGfxFileData
) {
    cgl::Results result = cgl::Results::Fail;
    size_t dataBlockOffset = gfxResInfo.dataOffset +
                             sizeof(GraphicsFileDataHeader);
    size_t dataBlockSize = header.dataSize -
                           sizeof(GraphicsFileDataHeader);

    // check PUK2's header
    if (header.flag & GraphicsDataFlags::DataVerPUK2) {
        GraphicsFileDataHeader_PUK2 header_puk2;
        result = this->read(dataBlockOffset, sizeof(header_puk2), &header_puk2);

        if (result != cgl::Results::Success) {
            LOGE("Failed to read PUK2's header, PUK1 header : [{}]",
                 toString(header));
            return result;
        }

        // update size of data block & offset
        dataBlockOffset += sizeof(GraphicsFileDataHeader_PUK2);
    }

    // update temporary block is necessary
    if (dataBlockSize > tempBlock_.size()) {
        tempBlock_.resize(dataBlockSize);
    }

    // read data block
    result = this->read(dataBlockOffset, dataBlockSize, tempBlock_.data());
    if (result != cgl::Results::Success) {
        LOGE("Failed to read data block in this header");
        return result;
    }

    // decompress the graphic resource
    uint32_t graphicsDataSize = header.width * header.height;
    auto graphicsData = std::make_unique<uint8_t[]>(graphicsDataSize);
    assert(graphicsData != nullptr);

    if (header.flag & GraphicsDataFlags::CompressedData) {
        char errorMsgBuf[1024];
        if (!Decompress(
                tempBlock_.data(),
                dataBlockSize,
                graphicsData.get(),
                graphicsDataSize,
                errorMsgBuf,
                sizeof(errorMsgBuf))) {
            LOGE("Failed to do decompress for this data block, msg {}",
                 errorMsgBuf);
        }
    } else {
        // Check the data size match or not for decompressed data.
        assert(header.dataSize == static_cast<uint32_t>(graphicsDataSize));
        memcpy(graphicsData.get(), tempBlock_.data(), header.dataSize);
    }

    // assign palette data
    std::optional<cgl::PaletteData256> palette;
    if (header.flag & GraphicsDataFlags::DataVerPUK2) {
        palette = std::make_optional<cgl::PaletteData256>();

        result = this->read(dataBlockOffset + graphicsDataSize,
                            sizeof(cgl::PaletteData) * 256,
                            palette->data());
    }

    // dump data (debug)
#ifdef CGL_DEBUG_GRAPHICS_RESOURCE_FILE_DATA
    for (int dy = 0 ; dy < header.height ; dy++) {
        for (int dx = 0 ; dx < header.width ; dx++) {
            uint8_t code = graphicsData[dy * header.width +dx];
            if (code == 0) printf("_ ");
            else if (code < 50) printf("O ");
            else if (code < 100) printf("^ ");
            else if (code < 150) printf("- ");
            else if (code < 200) printf("X ");
            else printf("+ ");
        }
        printf("\n");
    }
#endif

    // set output data with empty palette
    *pGfxFileData = cgl::GraphicsResourceData{
        .version = this->createInfo().version,
        .width = header.width,
        .height = header.height,
        .data = std::move(graphicsData),
        .paletteData = palette
    };


    return result;
}

cgl::Results GraphicsDataReaderImpl::query(
    const cgl::GraphicsResourceInfo& gfxResInfo,
    cgl::GraphicsResourceData*       pGfxResData
) {
    // Check args
    if (pGfxResData == nullptr) {
        return cgl::Results::InvalidArgs;
    }

    if (gfxResInfo.version != createInfo().version) {
        LOGE("Attempted to read graphic data from a mismatched version: the "
             "reader was created for `{}`, but the query requested data from "
             "`{}`",
             cgl::GetString(createInfo().version),
             cgl::GetString(gfxResInfo.version));
        return cgl::Results::InvalidArgs;
    }

    // Read data header from file. We will over read the size of header with
    // PUK2's struct and it would be safe since the PUK2's header is extended
    // from PUK1' header.
    GraphicsFileDataHeader header;
    auto result = this->read(gfxResInfo.dataOffset, sizeof(header), &header);

    if (result != cgl::Results::Success) {
        LOGE("Failed to read graphic data header from file, "
             "graphic index {} / map index {}",
             gfxResInfo.gfxBasedsIdx.value, gfxResInfo.mapBasedsIdx.value);
        return cgl::Results::Fail;
    }

    // verify the header
    if ((header.code[0] != 'R') || (header.code[1] != 'D')) {
        LOGE("Data header verification fail : {}", toString(header));
        return cgl::Results::Fail;
    }

    return ReadDataFromPUK(gfxResInfo, header, pGfxResData);
}

//------------------------------------------------------------------------------
// cgl::IGraphicsResourceFileDataReader
//------------------------------------------------------------------------------
cgl::IGraphicsResourceFileDataReader::Ptr
cgl::IGraphicsResourceFileDataReader::create(
    const cgl::IGraphicsResourceFileDataReader::CreateInfo& createInfo
) {
    if (createInfo.pSettings == nullptr) {
        return nullptr;
    }

    if (createInfo.version == cgl::CrossGateVersion::CG_VERSION_UNKNOWN) {
        return nullptr;
    }

    return std::make_unique<GraphicsDataReaderImpl>(createInfo);
}
