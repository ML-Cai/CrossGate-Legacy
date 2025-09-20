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
#include "cgl/assets/graphics_resource_file_data_reader.h"
#include "cgl/settings/settings.h"
#include "cgl/utils/filesystem.h"
#include "cgl/trace/logger.h"
#include "assets_path_info.h"


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
       << ", width " << h.width
       << ", height " << h.height
       << ", dataSize " << h.dataSize;

    return ss.str();
}

constexpr size_t HEX_100 = 0x100;    // 0x100
constexpr size_t HEX_10000 = 0x10000;    // 0x10000

enum OpCode : uint8_t {
    CODE_RAW_DATA_SMALL     = 0x0,
    CODE_RAW_DATA_MEDIUM    = 0x1,
    CODE_RAW_DATA_LARGE     = 0x2,
    CODE_REPEAT_BYTE_SMALL  = 0x8,
    CODE_REPEAT_BYTE_MEDIUM = 0x9,
    CODE_REPEAT_BYTE_LARGE  = 0xA,
    CODE_REPEAT_ZERO_SMALL  = 0xC,
    CODE_REPEAT_ZERO_MEDIUM = 0xD,
    CODE_REPEAT_ZERO_LARGE  = 0xE
};

//
// Decompress algorithm : https://cgsword.com/filesystem_graphicmap.htm
//
bool Decompress(
    const uint8_t* pRawDataBlock,
    const size_t   rawDataBlockSize,
    uint8_t*       pGfxData,
    const size_t   gfxDataSize,
    const size_t   paletteDataSize
) {
    if (!pRawDataBlock || !pGfxData) {
        LOGE("Decompress error: One or more input pointers are null.");
        return false;
    }

    const size_t totalOutputSize = gfxDataSize + paletteDataSize;
    uint8_t* pDst          = pGfxData;
    uint8_t* pDstEnd       = pGfxData + totalOutputSize;
    const uint8_t* pRaw    = pRawDataBlock;
    const uint8_t* pRawEnd = pRawDataBlock + rawDataBlockSize;
    bool isValidBlock      = true;

    while (pDst < pDstEnd) {
        // safe check
        if (pRaw >= pRawEnd) {
            LOGE("Unexpected end of raw data block when read the instruction");
            return false;
        }

        const size_t remainingDstSize = static_cast<size_t>(pDstEnd - pDst);
        const uint8_t instructionByte = *pRaw++;
        const uint8_t code = instructionByte >> 4;
        const uint8_t a = instructionByte & 0x0F;
        size_t count = 0;
        uint8_t value_to_repeat = 0;

        auto ENSURE_RAW_BYTES_AVAILABLE = [&](size_t  reqBytes,
                                              uint8_t code,
                                              auto&&  onSuccess) -> bool {
            if (static_cast<size_t>(pRawEnd - pRaw) < reqBytes) {
                LOGE("Unexpected end of raw data block when processing "
                     "instruction (0x" << std::hex << code << ")");
                return false;
            }
            onSuccess();
            return true;
        };

        // decompress the data block
        switch (static_cast<OpCode>(code)) {
        case CODE_RAW_DATA_SMALL:
            // 0a | xx :
            //  Read HEX(a) count of XX
            count = a;
            break;

        case CODE_RAW_DATA_MEDIUM: {
            // 1a | bb | xx :
            //  Read HEX(a * 100 + bb) count of xx
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(1, code, [&] {
                const uint8_t bb = *pRaw++;
                count = static_cast<size_t>(a) * HEX_100 + bb;
            });
            break;
        }

        case CODE_RAW_DATA_LARGE: {
            // 2a | bb | cc | xx :
            //  Read HEX(a * 10000 + bb * 100 + cc) count of xx
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(2, code, [&] {
                const uint8_t bb = *pRaw++;
                const uint8_t cc = *pRaw++;
                count = static_cast<size_t>(a) * HEX_10000 +
                        static_cast<size_t>(bb) * HEX_100 +
                        cc;
            });
            break;
        }

        case CODE_REPEAT_BYTE_SMALL: {
            // 8a | xx :
            //  Repeat HEX(a) count of transparent
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(1, code, [&] {
                value_to_repeat = *pRaw++;
                count = a;
            });
            break;
        }

        case CODE_REPEAT_BYTE_MEDIUM: {
            // 9a | xx | bb :
            //  Repeat HEX(a * 100 + bb) count of xx
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(2, code, [&] {
                value_to_repeat = *pRaw++;
                const uint8_t bb = *pRaw++;
                count = static_cast<size_t>(a) * HEX_100 + bb;
            });
            break;
        }

        case CODE_REPEAT_BYTE_LARGE: {
            // Aa | xx | bb | cc :
            //  Repeat HEX(a * 10000 + bb * 100 + cc) count of xx
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(3, code, [&] {
                value_to_repeat = *pRaw++;
                const uint8_t bb = *pRaw++;
                const uint8_t cc = *pRaw++;
                count = static_cast<size_t>(a) * HEX_10000 +
                        static_cast<size_t>(bb) * HEX_100 +
                        cc;
            });
            break;
        }

        case CODE_REPEAT_ZERO_SMALL:
            // Ca :
            //  Repeat HEX(a) count of transparent
            count = a;
            value_to_repeat = 0;
            break;

        case CODE_REPEAT_ZERO_MEDIUM: {
            // Da | bb :
            //  Repeat HEX(a * 100 + bb) count of transparent
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(1, code, [&] {
                const uint8_t bb = *pRaw++;
                count = static_cast<size_t>(a) * HEX_100 + bb;
                value_to_repeat = 0;
            });
            break;
        }

        case CODE_REPEAT_ZERO_LARGE: {
            // Ea | bb | cc :
            //   Read HEX(a * 10000 + bb * 100 + cc) count of transparent
            isValidBlock = ENSURE_RAW_BYTES_AVAILABLE(2, code, [&] {
                const uint8_t bb = *pRaw++;
                const uint8_t cc = *pRaw++;
                count = static_cast<size_t>(a) * HEX_10000 +
                        static_cast<size_t>(bb) * HEX_100 +
                        cc;
                value_to_repeat = 0;
            });
            break;
        }

        default:
            LOGE("Decompress Error: Unknown header : 0x" << std::hex << code);
            return false;
        }

        // Workaround for boundary check, It's safe in most case for regular
        // CrossGate data, but for case such as CG_VERSION_PUK3_V1:3674, it
        // might over the bounary for unknow reason. This cdoe will handle the
        // overflow issue and dump a warning message.
        if (count > remainingDstSize) {
            LOGW("Decompress Error: header (0x" << std::hex << code << "), "
                 << "Destination buffer overflow. Requested " << count
                 << " bytes, but only " << remainingDstSize << " remaining."
                 );
            isValidBlock = false;
        }

        //  Invalid block, skip this block
        if (!isValidBlock) {
            break;
        }

        // copy or set the data
        if (code <= CODE_RAW_DATA_LARGE) {
            // boundary check
            if ((pRaw + count) > pRawEnd) {
                LOGW("Decompress Error: header (0x" << std::hex << code << "), "
                     << "Raw data block overflow during copy operation. "
                     << "Requested " << count << " bytes, but not enough.");
                return false;
            }
            memcpy(pDst, pRaw, count);
            pRaw += count;
        } else {
            memset(pDst, value_to_repeat, count);
        }
        pDst += count;
    }

    // skip invalid block.
    if (isValidBlock) {
        assert(pDst == pDstEnd);

        if (pDst != pDstEnd) {
            LOGE("Decompress Error: Decompression finished prematurely. "
                 "Expected " << totalOutputSize << " bytes, but filled "
                 << static_cast<size_t>(pDst - pGfxData));
            return false;
        }
    }

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
    auto resPaths  = cgl::AcquireCrossGateResourcePath(createInfo().version);
    if ((resPaths.version == cgl::CrossGateVersion::UNKNOWN) ||
        (resPaths.version != createInfo().version)) {
        LOGE("Fail to query resource path configurations of version : "
             << cgl::ToStr(createInfo().version));
        return cgl::Results::InvalidFile;
    }

    // setup exceptions to capture the fail reasons.
    std::filesystem::path fullPath =
        std::filesystem::path(pSettings->general.EngineRootPath) /
        std::filesystem::path(cgl::Settings::General::AssetsRelPath) /
        std::filesystem::path(resPaths.graphicsDataSubPath);
    LOGD("Load graphics data from file : " << fullPath.string());

    if (this->open(fullPath) != true) {
        LOGE("Failed to open graphic date file :" << fullPath.string());
        return cgl::Results::InvalidFile;
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
    size_t graphicsDataSize = header.width * header.height;
    size_t paletteDataSize = 0;
    size_t dataBlockOffset = gfxResInfo.dataOffset +
                             sizeof(GraphicsFileDataHeader);
    size_t dataBlockSize;

    if (header.flag & GraphicsDataFlags::CompressedData) {
        dataBlockSize = header.dataSize - sizeof(GraphicsFileDataHeader);
    } else {
        dataBlockSize = graphicsDataSize;
    }

    // check PUK2's header
    uint8_t* pPalette = nullptr;
    if (header.flag & GraphicsDataFlags::DataVerPUK2) {
        GraphicsFileDataHeader_PUK2 header_puk2;

        if (read(dataBlockOffset, sizeof(header_puk2), &header_puk2) == false) {
            LOGE("Failed to read PUK2's header, PUK1 header : [{}]"
                 << toString(header));
            return cgl::Results::Fail;
        }

        // update size of data block & offset
        dataBlockOffset += sizeof(GraphicsFileDataHeader_PUK2);
        paletteDataSize = header_puk2.paletteSize;
    }

    // update temporary block if necessary
    if (dataBlockSize > tempBlock_.size()) {
        tempBlock_.resize(dataBlockSize);
    }

    // read data block
    if (read(dataBlockOffset, dataBlockSize, tempBlock_.data()) == false) {
        LOGE("Failed to read data block for header : [{}]" << toString(header));
        return cgl::Results::Fail;
    }

    // decompress the graphic resource, allocate the data with the
    // `graphicsDataSize` & `paletteDataSize` since we might need to allocate
    // the private palette in the same time.
    auto resourceData = std::make_unique<uint8_t[]>(graphicsDataSize + paletteDataSize);
    assert(resourceData != nullptr);

    if (header.flag & GraphicsDataFlags::CompressedData) {
        if (!Decompress(tempBlock_.data(),
                        dataBlockSize,
                        resourceData.get(),
                        graphicsDataSize,
                        paletteDataSize)) {
            LOGE("Failed to do decompress the data block for the header : "
                 << toString(header));
            return cgl::Results::Fail;
        }
    } else {
        dataBlockSize = graphicsDataSize;
        memcpy(resourceData.get(), tempBlock_.data(), dataBlockSize);
    }

    // assign palette data
    if (paletteDataSize > 0) {
        pPalette = resourceData.get() + graphicsDataSize;
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
        .pData = std::move(resourceData),
        .pPaletteData = pPalette
    };


    return cgl::Results::Success;
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
             "reader was created for " << cgl::ToStr(createInfo().version)
             << ", but the query requested data from "
             << cgl::ToStr(gfxResInfo.version));
        return cgl::Results::InvalidArgs;
    }

    // Read data header from file. We will over read the size of header with
    // PUK2's struct and it would be safe since the PUK2's header is extended
    // from PUK1' header.
    GraphicsFileDataHeader header;

    if (read(gfxResInfo.dataOffset, sizeof(header), &header) == false) {
        LOGE("Failed to read graphic data header from file, "
             << "graphic S/N : " << gfxResInfo.gfxBasedsIdx.value << ","
             << "map S/N : " << gfxResInfo.mapBasedsIdx.value);
        return cgl::Results::Fail;
    }

    // verify the header
    if ((header.code[0] != 'R') || (header.code[1] != 'D')) {
        LOGE("Data header verification fail :" << toString(header));
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

    if (createInfo.version == cgl::CrossGateVersion::UNKNOWN) {
        return nullptr;
    }

    return std::make_unique<GraphicsDataReaderImpl>(createInfo);
}
