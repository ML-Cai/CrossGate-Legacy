// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <array>
#include <optional>
#include "cgl/common/formatters.h"
#include "cgl/assets/palette_reader.h"
#include "cgl/settings/settings.h"
#include "cgl/utils/filesystem.h"
#include "cgl/trace/logger.h"
#include "assets_path_info.h"

using cgl::PaletteData;

constexpr size_t MAX_PALETTE_COUNT = 256;
constexpr size_t COLORS_IN_PALETTE_FILE = 236;
constexpr size_t DEFAULT_PALETTE_OFFSET = 16;
constexpr size_t PALETTE_FILE_SIZE = sizeof(cgl::PaletteData) *
                                     COLORS_IN_PALETTE_FILE;

// 16 + 16 remains for the default palette data.
constexpr size_t COLORS_READ_FROM_PALETTE_FILE = 256 - 32;


// -----------------------------------------------------------------------------
// cgl::PaletteLoader
// -----------------------------------------------------------------------------
class PaletteReaderImpl : public cgl::IPaletteReader {
 public:
    explicit PaletteReaderImpl(const CreateInfo& createInfo);

    ~PaletteReaderImpl();

    void clearCache() noexcept override {
        for (auto& entry : cache_) entry.reset();
    }

    // Try to open specific palet_*.cgp data but not read data yet.
    cgl::Results read(cgl::EnvironmentPaletteTypes envPalette,
                      cgl::PaletteData256*         pPaletteData) override;

 private:
    struct CachedPalette {
        std::filesystem::path path;
        cgl::PaletteData256 data;
    };
    // Four environment palettes, at most 4 x 768 bytes of color payload.
    std::array<std::optional<CachedPalette>, 4> cache_;

    void appendDefaultpPaletteData(cgl::PaletteData256* pPaletteData) noexcept;

    cgl::Results readPaletteFile(const std::filesystem::path& fullPath,
                                 cgl::PaletteData256* pPaletteData);
};

PaletteReaderImpl::PaletteReaderImpl(const CreateInfo& createInfo)
    : cgl::IPaletteReader(createInfo) {
}

PaletteReaderImpl::~PaletteReaderImpl() {
}

void PaletteReaderImpl::appendDefaultpPaletteData(
    cgl::PaletteData256* pPaletteData
) noexcept {
    // setup default palette data
    (*pPaletteData)[0].BGR   = {0x00, 0x00, 0x00};
    (*pPaletteData)[1].BGR   = {0x00, 0x00, 0x80};
    (*pPaletteData)[2].BGR   = {0x00, 0x80, 0x00};
    (*pPaletteData)[3].BGR   = {0x00, 0x80, 0x80};
    (*pPaletteData)[4].BGR   = {0x80, 0x00, 0x00};
    (*pPaletteData)[5].BGR   = {0x80, 0x00, 0x80};
    (*pPaletteData)[6].BGR   = {0x80, 0x80, 0x00};
    (*pPaletteData)[7].BGR   = {0xC0, 0xC0, 0xC0};

    (*pPaletteData)[8].BGR   = {0xC0, 0xDC, 0xC0};
    (*pPaletteData)[9].BGR   = {0xF0, 0xCA, 0xA6};
    (*pPaletteData)[10].BGR  = {0x00, 0x00, 0xDE};
    (*pPaletteData)[11].BGR  = {0x00, 0x5F, 0xFF};
    (*pPaletteData)[12].BGR  = {0xA0, 0xFF, 0xFF};
    (*pPaletteData)[13].BGR  = {0xD2, 0x5F, 0x00};
    (*pPaletteData)[14].BGR  = {0xFF, 0xD2, 0x50};
    (*pPaletteData)[15].BGR  = {0x28, 0xE1, 0x28};

    (*pPaletteData)[240].BGR = {0x96, 0xC3, 0xF5};
    (*pPaletteData)[241].BGR = {0x5F, 0xA0, 0x1E};
    (*pPaletteData)[242].BGR = {0x46, 0x7D, 0xC3};
    (*pPaletteData)[243].BGR = {0x1E, 0x55, 0x9B};
    (*pPaletteData)[244].BGR = {0x37, 0x41, 0x46};
    (*pPaletteData)[245].BGR = {0x1E, 0x23, 0x28};
    (*pPaletteData)[246].BGR = {0xF0, 0xFB, 0xFF};
    (*pPaletteData)[247].BGR = {0xA5, 0x6E, 0x3A};

    (*pPaletteData)[248].BGR = {0x80, 0x80, 0x80};
    (*pPaletteData)[249].BGR = {0x00, 0x00, 0xFF};
    (*pPaletteData)[250].BGR = {0x00, 0xFF, 0x00};
    (*pPaletteData)[251].BGR = {0x00, 0xFF, 0xFF};
    (*pPaletteData)[252].BGR = {0xFF, 0x00, 0x00};
    (*pPaletteData)[253].BGR = {0xFF, 0x80, 0xFF};
    (*pPaletteData)[254].BGR = {0xFF, 0xFF, 0x00};
    (*pPaletteData)[255].BGR = {0xFF, 0xFF, 0xFF};
}

cgl::Results PaletteReaderImpl::readPaletteFile(
    const std::filesystem::path& fullPath,
    cgl::PaletteData256* pPaletteData
) try {
    cgl::FileInfo fileInfo = cgl::TryOpenBinaryFile(fullPath);
    if (fileInfo.isOpen() == false) {
        LOGE("Failed to open environment palette file : `" << fullPath.string()
             << "`, msg " << fileInfo.errorMsg);
        return cgl::Results::Fail;
    }

    // verify the size
    size_t bufferSize = GetFileSize(&fileInfo);
    if (bufferSize != PALETTE_FILE_SIZE) {
        LOGE("Invalid palette file size for `" << fullPath.string()
             << "`. Expected " << PALETTE_FILE_SIZE << ", got " << bufferSize);
        return cgl::Results::Fail;
    }

    // read all PaletteLoader data
    auto& stream = fileInfo.stream.value();
    stream.read(reinterpret_cast<char*>(pPaletteData->data() +
                                        DEFAULT_PALETTE_OFFSET),
                bufferSize);

    return cgl::Results::Success;
}
catch (const std::ios_base::failure&) {
    return cgl::Results::InvalidFile;
}

cgl::Results PaletteReaderImpl::read(
    cgl::EnvironmentPaletteTypes envPalette,
    cgl::PaletteData256*         pPaletteData
) {
    if (!pPaletteData) return cgl::Results::InvalidArgs;
    const auto index = static_cast<size_t>(envPalette);
    if (index >= cache_.size()) return cgl::Results::InvalidArgs;
    // Get file info.
    const auto pSettings = createInfo().pSettings;
    const auto resPath = cgl::AcquireEnvPalettePath(envPalette);
    if (resPath.paletteType != envPalette) {
        LOGE("Palette path mismatch for : " << envPalette);
        return cgl::Results::Fail;
    }

    auto fullPath =
        std::filesystem::path(pSettings->general.EngineRootPath) /
        std::filesystem::path(resPath.paletteFilePath);


    std::error_code pathError;
    fullPath = std::filesystem::absolute(fullPath, pathError).lexically_normal();
    if (pathError) return cgl::Results::InvalidFile;
    auto& entry = cache_[index];
    if (entry && entry->path == fullPath) {
        *pPaletteData = entry->data;
        return cgl::Results::Success;
    }
    LOGD("Loading env palette(" << envPalette << ") from file : " << fullPath.string());
    entry.reset(); // Do not retain an entry for a previous resource root.
    cgl::PaletteData256 palette{};
    const auto result = readPaletteFile(fullPath, &palette);
    if (result != cgl::Results::Success) return result;
    appendDefaultpPaletteData(&palette);
    entry.emplace(CachedPalette{std::move(fullPath), palette});
    *pPaletteData = palette;

    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
// cgl::IPaletteReader
// -----------------------------------------------------------------------------
cgl::IPaletteReader::Ptr cgl::IPaletteReader::create(
    const cgl::IPaletteReader::CreateInfo& createInfo
) {
    if (createInfo.pSettings == nullptr) {
        return nullptr;
    }

    return std::make_unique<PaletteReaderImpl>(createInfo);
}
