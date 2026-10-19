#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <unordered_map>
#include "cgl/assets/map_layout_reader.h"
#include "cgl/graphics/sprite_renderer.h"

namespace cgl {
namespace component { struct AssetsReaderState; }

struct GroundTile {
    uint32_t frame;
    int32_t offsetX, offsetY;
};
struct GroundMap {
    MapLayout layout;
    std::unordered_map<uint16_t, GroundTile> tiles;
    graphics::ISpriteRenderer::Ptr renderer;
    uint32_t frameWidth = 0, frameHeight = 0;
    double reachX = 0, reachY = 0;
};

// The engine retains uploaded maps across scene restarts and pending submissions.
// Clear only after GPU completion and scene exit, before destroying the device.
class GroundMapLibrary {
public:
    std::shared_ptr<const GroundMap> acquire(const std::filesystem::path& path,
        component::AssetsReaderState& readers, graphics::IDevice* device, graphics::IRenderPass* pass);
    void clear() { maps_.clear(); device_ = nullptr; pass_ = nullptr; }
private:
    std::map<std::filesystem::path, std::shared_ptr<const GroundMap>> maps_;
    graphics::IDevice* device_ = nullptr;
    graphics::IRenderPass* pass_ = nullptr;
};
}
