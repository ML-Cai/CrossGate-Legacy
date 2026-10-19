#include "cgl/assets/map_layout_reader.h"
#include <array>
#include <fstream>
#include <new>

namespace {
uint32_t Read32(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
        (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

class MapLayoutReader final : public cgl::IMapLayoutReader {
public:
    cgl::Results read(const std::filesystem::path& path, cgl::MapLayout* output) override {
        if (!output) return cgl::Results::InvalidArgs;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return cgl::Results::InvalidFile;
        const auto size = file.tellg();
        if (size < 20) return cgl::Results::InvalidFile;
        file.seekg(0);
        std::array<unsigned char, 20> header{};
        if (!file.read(reinterpret_cast<char*>(header.data()), header.size()) ||
            header[0] != 'M' || header[1] != 'A' || header[2] != 'P')
            return cgl::Results::InvalidFile;
        const auto width = Read32(header.data() + 12);
        const auto height = Read32(header.data() + 16);
        const uint64_t count = uint64_t(width) * height;
        if (!width || !height || width > 65536 || height > 65536 ||
            count > 4194304 || uint64_t(size) != 20 + count * 6)
            return cgl::Results::InvalidFile;
        try {
            cgl::MapLayout result{width, height, std::vector<cgl::MapCell>(static_cast<size_t>(count))};
            for (int plane = 0; plane < 3; ++plane) {
                for (auto& cell : result.cells) {
                    unsigned char bytes[2];
                    if (!file.read(reinterpret_cast<char*>(bytes), 2))
                        return cgl::Results::InvalidFile;
                    const auto value = static_cast<uint16_t>(bytes[0] | (uint16_t(bytes[1]) << 8));
                    if (plane == 0) cell.groundId = value;
                    else if (plane == 1) cell.objectId = value;
                    else cell.flags = value;
                }
            }
            *output = std::move(result);
            return cgl::Results::Success;
        } catch (const std::bad_alloc&) {
            return cgl::Results::OutOfMemory;
        }
    }
};
}

cgl::IMapLayoutReader::Ptr cgl::IMapLayoutReader::create() {
    return std::make_unique<MapLayoutReader>();
}
