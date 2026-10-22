#include "assets/animation_builder.h"
#include "assets/assets_components.h"
#include <algorithm>
#include <stdexcept>

namespace cgl {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
AnimationPixels AnimationBuilder::build(const AnimationRequest& request,
    component::AssetsReaderState& readers, uint64_t availableBytes, const std::atomic_bool* cancelled) {
    std::lock_guard readerLock(*readers.queryMutex);
    auto checkCancellation=[&] {
        if(cancelled && cancelled->load()) throw std::runtime_error("Animation decode cancelled");
    };
    checkCancellation();
    const auto version = request.version;
    require(version < CrossGateVersion::COUNT, "Invalid animation resource version");
    constexpr uint64_t maxBytes = 256ull * 1024 * 1024;
    constexpr size_t maxFramesPerMotion = 65536, maxDescriptionFrames = 1024 * 1024;
    require(request.direction < uint32_t(DirectionTypes::COUNT) &&
            request.motion < uint32_t(MotionTypes::COUNT), "Invalid animation direction or motion");
    const auto* infoSource = readers.pAnimeInfoReader[version].get();
    const auto* dataSource = readers.pAnimeDataReader[version].get();
    require(infoSource && dataSource && readers.pGfxInfoReader[version] &&
            readers.pGfxDataReader[version] && readers.pPaletteReader, "Animation readers are unavailable");
    if (!description_ || description_->serialNum.value != request.serial ||
        description_->serialNum.version != version ||
        infoReader_ != infoSource || dataReader_ != dataSource) {
        clear();
        AnimeResourceInfo info{};
        require(readers.pAnimeInfoReader[version]->query({version, request.serial}, &info) == Results::Success,
                "Animation serial number not found (check [Animation] SerialNum)");
        AnimeResourceData description{};
        require(readers.pAnimeDataReader[version]->query(info, &description) == Results::Success,
                "Could not read animation data");
        require(description.serialNum.value == request.serial && description.serialNum.version == version,
                "Animation description serial/version mismatch");
        require(description.motionMap.size() <= size_t(DirectionTypes::COUNT) * size_t(MotionTypes::COUNT),
                "Animation description exceeds motion budget");
        size_t totalFrames = 0;
        for (const auto& [key, motion] : description.motionMap) {
            require(motion.motionGraphicsSerialNums.size() <= maxFramesPerMotion &&
                    motion.motionGraphicsSerialNums.size() <= maxDescriptionFrames - totalFrames,
                    "Animation description exceeds frame budget");
            totalFrames += motion.motionGraphicsSerialNums.size();
        }
        description_ = std::move(description);
        infoReader_ = infoSource;
        dataReader_ = dataSource;
    }
    const auto motion = description_->motionMap.find(
        {DirectionTypes(request.direction), MotionTypes(request.motion)});
    require(motion != description_->motionMap.end(), "Requested direction/motion is unavailable for this character");
    const auto& clip = motion->second;
    require(!clip.motionGraphicsSerialNums.empty() && clip.duration > 0, "Animation has no frames or duration");

    // Plan from small metadata first. No decoded pixels or palette reads until
    // the full canvas and remaining texture payload budget have been validated.
    std::vector<GraphicsResourceInfo> frames;
    frames.reserve(clip.motionGraphicsSerialNums.size());
    int64_t left = 0, top = 0, right = 0, bottom = 0;
    for (auto serial : clip.motionGraphicsSerialNums) {
        checkCancellation();
        GraphicsResourceInfo info{};
        require(readers.pGfxInfoReader[version]->query(
            {GraphicsResourceSerialNumTypes::GraphicsSerialNum, version, serial}, &info) == Results::Success,
            "Could not find animation frame info");
        require(info.width > 0 && info.height > 0 && info.width <= 4096 && info.height <= 4096,
                "Invalid animation frame dimensions");
        left = std::min(left, int64_t(info.offsetX));
        top = std::min(top, int64_t(info.offsetY));
        right = std::max(right, int64_t(info.offsetX) + info.width);
        bottom = std::max(bottom, int64_t(info.offsetY) + info.height);
        frames.push_back(info);
    }
    const auto width = right - left, height = bottom - top;
    const auto budget = std::min(availableBytes, maxBytes);
    require(width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
            frames.size() <= budget / (uint64_t(width) * height * 4),
            "Animation preview exceeds remaining texture memory budget");
    checkCancellation();
    PaletteData256 palette{};
    require(readers.pPaletteReader->read(EnvironmentPaletteTypes::Daytime, &palette) == Results::Success,
            "Could not read daytime palette");
    const size_t frameBytes = size_t(width * height) * 4;
    AnimationPixels result{uint32_t(width), uint32_t(height), uint32_t(frames.size()), clip.duration,
                           int32_t(left), int32_t(top), std::vector<uint8_t>(frameBytes * frames.size(), 0)};
    for (size_t f = 0; f < frames.size(); ++f) {
        checkCancellation();
        // The indexed pixels for this frame die before the next decode begins.
        GraphicsResourceData data{};
        const auto& info = frames[f];
        require(readers.pGfxDataReader[version]->query(info, &data) == Results::Success,
                "Could not decode animation frame");
        require(data.pData && data.width == info.width && data.height == info.height,
                "Invalid animation frame dimensions");
        const auto* colors = data.pPaletteData ? data.pPaletteData :
            reinterpret_cast<const uint8_t*>(palette.data());
        for (int y = 0; y < data.height; ++y) {
            checkCancellation();
            for (int x = 0; x < data.width; ++x) {
                const auto index = data.pData[(data.height - 1 - y) * size_t(data.width) + x];
                const size_t dstX = size_t(int64_t(info.offsetX) - left + x);
                const size_t dstY = size_t(int64_t(info.offsetY) - top + y);
                auto* pixel = result.rgba.data() + f * frameBytes + (dstY * width + dstX) * 4;
                pixel[0] = colors[index * 3 + 2];
                pixel[1] = colors[index * 3 + 1];
                pixel[2] = colors[index * 3];
                pixel[3] = index == 0 ? 0 : 255;
            }
        }
    }
    return result;
}
}
