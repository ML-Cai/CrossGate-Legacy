#include "assets/ground_map_library.h"
#include "assets/assets_components.h"
#include "cgl/trace/logger.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace cgl {
std::shared_ptr<const GroundMap> GroundMapLibrary::acquire(const std::filesystem::path& path,
    component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Ground map library requires one live device/render pass");
    const auto key = std::filesystem::absolute(path).lexically_normal();
    if (const auto found = maps_.find(key); found != maps_.end()) return found->second;
    auto result = std::make_shared<GroundMap>();
    if (IMapLayoutReader::create()->read(key, &result->layout) != Results::Success)
        throw std::runtime_error("Could not read MAP layout: " + key.string());
    constexpr auto version = CrossGateVersion::CG_VERSION_Classic;
    auto* infoReader = readers.pGfxInfoReader[version].get();
    auto* dataReader = readers.pGfxDataReader[version].get();
    if (!infoReader || !dataReader || !readers.pPaletteReader)
        throw std::runtime_error("Ground map requires initialized Classic readers");
    PaletteData256 palette;
    if (readers.pPaletteReader->read(EnvironmentPaletteTypes::Daytime, &palette) != Results::Success)
        throw std::runtime_error("Could not read ground map palette");
    std::set<uint16_t> ids;
    for (const auto& cell : result->layout.cells) if (cell.groundId) ids.insert(cell.groundId);
    struct Tile { GraphicsResourceInfo info; GraphicsResourceData data; };
    std::vector<Tile> tiles;
    for (const auto id : ids) {
        Tile tile{};
        if (infoReader->query({GraphicsResourceSerialNumTypes::MapSerialNum, version, id}, &tile.info) != Results::Success)
            throw std::runtime_error("Classic ground map ID not found: " + std::to_string(id));
        if (tile.info.width <= 0 || tile.info.height <= 0 || tile.info.width > 512 || tile.info.height > 512)
            throw std::runtime_error("Ground tile exceeds supported dimensions");
        result->frameWidth = std::max(result->frameWidth, uint32_t(tile.info.width));
        result->frameHeight = std::max(result->frameHeight, uint32_t(tile.info.height));
        const uint64_t frameBytes = uint64_t(result->frameWidth) * result->frameHeight * 4;
        if (tiles.size() + 1 > (128ull * 1024 * 1024) / frameBytes)
            throw std::runtime_error("Ground map exceeds texture memory budget");
        if (dataReader->query(tile.info, &tile.data) != Results::Success || !tile.data.pData ||
            tile.data.width != tile.info.width || tile.data.height != tile.info.height)
            throw std::runtime_error("Could not decode ground tile: " + std::to_string(id));
        result->reachX = std::max({result->reachX, std::abs(double(tile.info.offsetX)),
            std::abs(double(tile.info.offsetX) + tile.info.width)});
        result->reachY = std::max({result->reachY, std::abs(double(tile.info.offsetY)),
            std::abs(double(tile.info.offsetY) + tile.info.height)});
        result->tiles.emplace(id, GroundTile{uint32_t(tiles.size()), tile.info.offsetX, tile.info.offsetY});
        tiles.push_back(std::move(tile));
    }
    if (!tiles.empty()) {
        const uint64_t frameBytes = uint64_t(result->frameWidth) * result->frameHeight * 4;
        std::vector<uint8_t> rgba(size_t(frameBytes * tiles.size()), 0);
        for (size_t f = 0; f < tiles.size(); ++f) {
            const auto& tile = tiles[f];
            const auto* colors = tile.data.pPaletteData ? tile.data.pPaletteData :
                reinterpret_cast<const uint8_t*>(palette.data());
            for (int y = 0; y < tile.data.height; ++y) for (int x = 0; x < tile.data.width; ++x) {
                const auto index = tile.data.pData[(tile.data.height - 1 - y) * size_t(tile.data.width) + x];
                auto* dst = rgba.data() + size_t(f * frameBytes) + (size_t(y) * result->frameWidth + x) * 4;
                dst[0] = colors[index * 3 + 2]; dst[1] = colors[index * 3 + 1];
                dst[2] = colors[index * 3]; dst[3] = index ? 255 : 0;
            }
        }
        result->renderer = graphics::ISpriteRenderer::create(device, pass,
            result->frameWidth, result->frameHeight, uint32_t(tiles.size()), rgba);
        if (!result->renderer) throw std::runtime_error("Could not upload ground tiles (check device texture limits)");
    }
    maps_.emplace(key, result);
    device_ = device; pass_ = pass;
    LOGI("Ground map ready: " << result->layout.width << "x" << result->layout.height << ", tiles=" << tiles.size());
    return result;
}
}
