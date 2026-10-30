#include "cgl/assets/animation_decoder.h"
#include "cgl/assets/anime_info_reader.h"
#include "cgl/assets/anime_data_reader.h"
#include "cgl/assets/graphics_info_reader.h"
#include "cgl/assets/graphics_data_reader.h"
#include "cgl/assets/palette_reader.h"
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace cgl {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}
AnimationDecoder::Plan AnimationDecoder::prepare(const AnimationRequest& request,
    const AnimationReaders& readers,const std::atomic_bool* cancelled) {
    auto checkCancellation=[&] {
        if(cancelled && cancelled->load()) throw std::runtime_error("Animation decode cancelled");
    };
    checkCancellation();
    const auto version = request.version;
    require(version < CrossGateVersion::COUNT, "Invalid animation resource version");
    constexpr size_t maxFramesPerMotion = 65536, maxDescriptionFrames = 1024 * 1024;
    require(request.direction < uint32_t(DirectionTypes::COUNT) &&
            request.motion < uint32_t(MotionTypes::COUNT), "Invalid animation direction or motion");
    const auto* infoSource = readers.animeInfo;
    const auto* dataSource = readers.animeData;
    require(infoSource && dataSource && readers.graphicsInfo &&
            readers.graphicsData && readers.palette, "Animation readers are unavailable");
    if (!description_ || description_->serialNum.value != request.serial ||
        description_->serialNum.version != version ||
        infoReader_ != infoSource || dataReader_ != dataSource) {
        clear();
        AnimeResourceInfo info{};
        require(readers.animeInfo->query({version, request.serial}, &info) == Results::Success,
                "Animation serial number not found (check [Animation] SerialNum)");
        AnimeResourceData description{};
        require(readers.animeData->query(info, &description) == Results::Success,
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
    const bool mirrorX=(clip.extendedFlags&0x00010000u)!=0;
    require(!clip.motionGraphicsSerialNums.empty() && clip.duration > 0, "Animation has no frames or duration");

    // Plan from small metadata first. No decoded pixels or palette reads until
    // the full canvas and remaining texture payload budget have been validated.
    std::vector<GraphicsResourceInfo> frames;
    frames.reserve(clip.motionGraphicsSerialNums.size());
    int64_t left = 0, top = 0, right = 0, bottom = 0;
    for (auto serial : clip.motionGraphicsSerialNums) {
        checkCancellation();
        GraphicsResourceInfo info{};
        require(readers.graphicsInfo->query(
            {GraphicsResourceSerialNumTypes::GraphicsSerialNum, version, serial}, &info) == Results::Success,
            "Could not find animation frame info");
        require(info.width > 0 && info.height > 0 && info.width <= 4096 && info.height <= 4096,
                "Invalid animation frame dimensions");
        if(mirrorX) {
            const int64_t reflected=-int64_t(info.offsetX)-info.width;
            require(reflected>=std::numeric_limits<int32_t>::min() && reflected<=std::numeric_limits<int32_t>::max(),
                    "Mirrored animation offset is outside int32");
            info.offsetX=int32_t(reflected);
        }
        left = std::min(left, int64_t(info.offsetX));
        top = std::min(top, int64_t(info.offsetY));
        right = std::max(right, int64_t(info.offsetX) + info.width);
        bottom = std::max(bottom, int64_t(info.offsetY) + info.height);
        frames.push_back(info);
    }
    const auto width = right - left, height = bottom - top;
    require(width > 0 && height > 0 && width <= 4096 && height <= 4096,
            "Invalid animation canvas dimensions");
    const uint64_t requiredBytes=uint64_t(width)*height*4*frames.size();
    require(requiredBytes<=MaxBytes,"Animation exceeds per-clip texture memory limit");
    const auto frameCount=uint32_t(frames.size());
    return {{uint32_t(width),uint32_t(height),frameCount,clip.duration,
             int32_t(left),int32_t(top),{}},std::move(frames),requiredBytes,request,mirrorX};
}
uint64_t AnimationDecoder::requiredBytes(const AnimationRequest& request,const AnimationReaders& readers,const std::atomic_bool* cancelled) {
    return prepare(request,readers,cancelled).bytes;
}
AnimationPixels AnimationDecoder::build(const AnimationRequest& request,
    const AnimationReaders& readers,uint64_t availableBytes,const std::atomic_bool* cancelled) {
    auto plan=prepare(request,readers,cancelled);
    return render(plan,readers,0,plan.frames.size(),availableBytes,cancelled);
}
AnimationPixels AnimationDecoder::buildFrame(const Plan& plan,size_t index,const AnimationReaders& readers,uint64_t availableBytes) {
    if(index>=plan.frames.size())throw std::out_of_range("Animation frame index is outside the plan");
    return render(plan,readers,index,1,availableBytes,nullptr);
}
AnimationPixels AnimationDecoder::render(const Plan& plan,const AnimationReaders& readers,
    size_t first,size_t count,uint64_t availableBytes,const std::atomic_bool* cancelled) {
    require(readers.graphicsData && readers.palette,"Animation frame readers are unavailable");
    const uint64_t bytes=uint64_t(plan.canvas.width)*plan.canvas.height*4*count;
    if(bytes>availableBytes)throw AnimationBudgetExceeded(bytes);
    auto checkCancellation=[&] {
        if(cancelled && cancelled->load()) throw std::runtime_error("Animation decode cancelled");
    };
    checkCancellation();
    auto result=plan.canvas;result.frameCount=uint32_t(count);const auto& frames=plan.frames;
    const auto width=result.width,height=result.height;
    const auto left=result.offsetX,top=result.offsetY;
    PaletteData256 palette{};
    require(readers.palette->read(EnvironmentPaletteTypes::Daytime, &palette) == Results::Success,
            "Could not read daytime palette");
    GraphicsResourceData hiddenPalette{};
    if(plan.request.version>=CrossGateVersion::CG_VERSION_PUK2_V1) {
        auto* info=readers.hiddenPaletteInfo?readers.hiddenPaletteInfo:readers.graphicsInfo;
        auto* data=readers.hiddenPaletteData?readers.hiddenPaletteData:readers.graphicsData;
        GraphicsResourceInfo marker{};
        if(info && info->query({GraphicsResourceSerialNumTypes::MapSerialNum,info->createInfo().version,plan.request.serial},&marker)==Results::Success) {
            require(data && data->query(marker,&hiddenPalette)==Results::Success,"Could not decode animation palette");
        }
    }
    const size_t frameBytes = size_t(width * height) * 4;
    result.rgba.resize(size_t(bytes),0);
    for (size_t f = 0; f < count; ++f) {
        checkCancellation();
        // The indexed pixels for this frame die before the next decode begins.
        GraphicsResourceData data{};
        const auto& info = frames[first+f];
        require(readers.graphicsData->query(info, &data) == Results::Success,
                "Could not decode animation frame");
        require(data.pData && data.width == info.width && data.height == info.height,
                "Invalid animation frame dimensions");
        const auto* colors = data.pPaletteData ? data.pPaletteData : hiddenPalette.pPaletteData ? hiddenPalette.pPaletteData :
            reinterpret_cast<const uint8_t*>(palette.data());
        for (int y = 0; y < data.height; ++y) {
            checkCancellation();
            for (int x = 0; x < data.width; ++x) {
                const auto sourceX=plan.mirrorX?data.width-1-x:x;
                const auto index = data.pData[(data.height - 1 - y) * size_t(data.width) + sourceX];
                if(!data.pPaletteData && hiddenPalette.pPaletteData)
                    require(size_t(index)*3+2<hiddenPalette.paletteBytes,"Animation palette is too short for frame indices");
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
