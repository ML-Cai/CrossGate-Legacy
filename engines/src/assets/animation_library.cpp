#include "assets/animation_library.h"
#include "assets/assets_components.h"
#include "cgl/trace/logger.h"
#include <algorithm>
#include <stdexcept>

namespace cgl {
std::shared_ptr<const AnimationClip> AnimationLibrary::acquire(
    const AnimationRequest& request, component::AssetsReaderState& readers,
    graphics::IDevice* device, graphics::IRenderPass* pass) {
    if (!device || !pass || (device_ && device_ != device) || (pass_ && pass_ != pass))
        throw std::invalid_argument("Animation library requires one live device/render pass");
    if (const auto found = clips_.find(request); found != clips_.end()) return found->second;
    constexpr auto version = CrossGateVersion::CG_VERSION_Classic;
    const auto serial = request.serial;
    if (request.direction >= uint32_t(DirectionTypes::COUNT) ||
        request.motion >= uint32_t(MotionTypes::COUNT)) {
        throw std::runtime_error("Invalid animation direction or motion in settings.ini");
    }
    auto require = [](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    };
    AnimeResourceInfo info{};
    require(readers.pAnimeInfoReader[version]->query({version, serial}, &info) == Results::Success,
            "Animation serial number not found (check [Animation] SerialNum)");
    AnimeResourceData animation;
    require(readers.pAnimeDataReader[version]->query(info, &animation) == Results::Success,
            "Could not read animation data");
    const AnimeDataMapKey key{DirectionTypes(request.direction), MotionTypes(request.motion)};
    const auto motion = animation.motionMap.find(key);
    require(motion != animation.motionMap.end(), "Requested direction/motion is unavailable for this character");
    const auto& clip = motion->second;
    require(!clip.motionGraphicsSerialNums.empty() && clip.duration > 0, "Animation has no frames or duration");
    PaletteData256 palette;
    require(readers.pPaletteReader->read(EnvironmentPaletteTypes::Daytime, &palette) == Results::Success,
            "Could not read daytime palette");

    struct Frame { GraphicsResourceInfo info; GraphicsResourceData data; };
    std::vector<Frame> frames;
    int64_t left = 0, top = 0, right = 0, bottom = 0;
    for (auto serialNum : clip.motionGraphicsSerialNums) {
        Frame frame{};
        require(readers.pGfxInfoReader[version]->query(
            {GraphicsResourceSerialNumTypes::GraphicsSerialNum, version, serialNum}, &frame.info) == Results::Success,
            "Could not find animation frame info");
        require(readers.pGfxDataReader[version]->query(frame.info, &frame.data) == Results::Success,
            "Could not decode animation frame");
        require(frame.data.pData && frame.data.width > 0 && frame.data.height > 0 &&
                frame.data.width == frame.info.width && frame.data.height == frame.info.height,
                "Invalid animation frame dimensions");
        left = std::min(left, int64_t(frame.info.offsetX));
        top = std::min(top, int64_t(frame.info.offsetY));
        right = std::max(right, int64_t(frame.info.offsetX) + frame.data.width);
        bottom = std::max(bottom, int64_t(frame.info.offsetY) + frame.data.height);
        frames.push_back(std::move(frame));
    }
    const auto width = right - left, height = bottom - top;
    // Bound CPU allocations before constructing the aligned texture array.
    constexpr uint64_t maxPreviewBytes = 256ull * 1024 * 1024;
    require(width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
            frames.size() <= maxPreviewBytes / (uint64_t(width) * height * 4),
            "Animation preview exceeds texture memory budget");
    const size_t frameBytes = size_t(width * height) * 4;
    std::vector<uint8_t> rgba(frameBytes * frames.size(), 0);
    for (size_t f = 0; f < frames.size(); ++f) {
        const auto& frame = frames[f];
        auto colors = reinterpret_cast<const uint8_t*>(palette.data());
        if (frame.data.pPaletteData) colors = frame.data.pPaletteData;
        for (int y = 0; y < frame.data.height; ++y) {
            for (int x = 0; x < frame.data.width; ++x) {
                // Native rows are bottom-up, matching AnimeViewer's img[::-1].
                const auto index = frame.data.pData[(frame.data.height - 1 - y) * size_t(frame.data.width) + x];
                const size_t dstX = size_t(int64_t(frame.info.offsetX) - left + x);
                const size_t dstY = size_t(int64_t(frame.info.offsetY) - top + y);
                auto* pixel = rgba.data() + f * frameBytes + (dstY * width + dstX) * 4;
                pixel[0] = colors[index * 3 + 2];
                pixel[1] = colors[index * 3 + 1];
                pixel[2] = colors[index * 3];
                pixel[3] = index == 0 ? 0 : 255;
            }
        }
    }
    auto renderer = graphics::ISpriteRenderer::create(device, pass,
        uint32_t(width), uint32_t(height), uint32_t(frames.size()), rgba);
    require(bool(renderer), "Could not upload animation or create sprite pipeline");
    auto result = std::make_shared<AnimationClip>();
    result->renderer = std::move(renderer);
    result->offsetX = int32_t(left);
    result->offsetY = int32_t(top);
    result->width = uint32_t(width);
    result->height = uint32_t(height);
    result->frameCount = uint32_t(frames.size());
    result->durationMs = clip.duration;
    clips_.emplace(request, result);
    device_ = device;
    pass_ = pass;
    LOGI("Character animation ready: serial=" << serial << ", frames=" << frames.size()
         << ", duration=" << clip.duration << "ms, canvas=" << width << "x" << height);
    return result;
}
}
