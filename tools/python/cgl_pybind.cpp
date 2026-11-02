// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <vector>
#include <algorithm>
#include <filesystem>
#include <type_traits>
#include <cstdint>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "cgl/common/assets.h"
#include "cgl/common/direction.h"
#include "cgl/common/motion.h"
#include "cgl/common/formatters.h"
#include "cgl/settings/settings.h"
#include "cgl/assets/graphics_info_reader.h"
#include "cgl/assets/graphics_data_reader.h"
#include "cgl/assets/anime_info_reader.h"
#include "cgl/assets/anime_data_reader.h"
#include "cgl/assets/palette_reader.h"
#include "cgl/trace/logger.h"
#include "holders.h"
#include "map_snapshot.h"
#include "map_preview.h"
#include "cgl/assets/animation_decoder.h"

namespace py = pybind11;
using namespace pybind11::literals;


namespace {

// -----------------------------------------------------------------------------
template<typename T>
inline void remove(std::vector<T>* v, T target) {
    auto it = std::find(v->begin(), v->end(), target);
    if (it != v->end()) {
        v->erase(it);
    }
}

// -----------------------------------------------------------------------------
template <typename T>
std::string type_name() {
    return typeid(T).name();
}

// -----------------------------------------------------------------------------
std::vector<cgl::EnvironmentPaletteTypes> GetTypes_EnvironmentPaletteTypes() {
    std::vector<cgl::EnvironmentPaletteTypes> result;
#define CGL_X(name) result.push_back(cgl::EnvironmentPaletteTypes::name);
    CGL_ENVIRONMENT_PALETTE_TYPES_ENUM_LIST
#undef CGL_X
    remove(&result, cgl::EnvironmentPaletteTypes::Unknown);

    return result;
}

// -----------------------------------------------------------------------------
std::vector<cgl::CrossGateVersion> GetTypes_CrossGateVersion() {
    std::vector<cgl::CrossGateVersion> result;
#define CGL_X(name) result.push_back(cgl::CrossGateVersion::name);
    CGL_VERSION_TYPES_VALID_LIST
#undef CGL_X

    return result;
}

// -----------------------------------------------------------------------------
struct AnimationPlan {
    cgl::AnimationDecoder::Plan plan;
    cgl::CrossGateVersion version;
    std::weak_ptr<uint8_t> epoch;
    py::dict metadata() const {
        const auto& p=plan.metadata();
        return py::dict("width"_a=p.width,"height"_a=p.height,"frame_count"_a=p.frameCount,
            "duration_ms"_a=p.durationMs,"offset_x"_a=p.offsetX,"offset_y"_a=p.offsetY,
            "clip_bytes"_a=plan.requiredBytes(),"frame_bytes"_a=uint64_t(p.width)*p.height*4);
    }
};

class Session {
public:
    explicit Session(const std::string& engineRootPath = "", size_t cacheBudgetBytes = 4 * 1024 * 1024);
    std::unique_ptr<AnimationPlan> plan_animation(cgl::CrossGateVersion version,uint32_t serial,cgl::DirectionTypes direction,cgl::MotionTypes motion) {
        ensureOpen();
        if(version>=cgl::CrossGateVersion::COUNT)throw std::invalid_argument("Invalid resource version");
        auto plan=animationDecoder_.prepare({serial,uint32_t(direction),uint32_t(motion),version},animationReaders(version));
        if(!animationEpoch_)animationEpoch_=std::make_shared<uint8_t>(0);
        return std::make_unique<AnimationPlan>(AnimationPlan{std::move(plan),version,animationEpoch_});
    }
    py::array_t<uint8_t> decode_animation_frame(const AnimationPlan& plan,size_t index,uint64_t maxBytes) {
        ensureOpen();
        auto epoch=plan.epoch.lock();
        if(!epoch || epoch!=animationEpoch_)throw std::runtime_error("Animation plan is expired or belongs to another Session");
        if(!maxBytes || maxBytes>4ull*1024*1024)throw std::invalid_argument("max_bytes must be 1..4194304 per frame");
        auto pixels=animationDecoder_.buildFrame(plan.plan,index,animationReaders(plan.version),maxBytes);
        auto buffer=std::make_unique<std::vector<uint8_t>>(std::move(pixels.rgba));
        py::capsule owner(buffer.get(),[](void* p){delete static_cast<std::vector<uint8_t>*>(p);});
        auto* data=buffer->data();buffer.release();
        return py::array_t<uint8_t>({py::ssize_t(pixels.height),py::ssize_t(pixels.width),py::ssize_t(4)},
            {py::ssize_t(pixels.width)*4,py::ssize_t(4),py::ssize_t(1)},data,owner);
    }
    std::unique_ptr<cgl::py::MapSnapshot> load_map(const std::string& filePath) {
        ensureOpen();
        if (filePath.empty()) throw std::invalid_argument("file_path must not be empty");
        auto path = std::filesystem::u8path(filePath);
        if (path.is_relative()) path = std::filesystem::u8path(pSettings_->general.EngineRootPath) / path;
        return cgl::py::MapSnapshot::load(path);
    }
    std::unique_ptr<cgl::py::MapPreview> load_map_preview(const std::string& filePath) {
        ensureOpen();
        if(filePath.empty())throw std::invalid_argument("file_path must not be empty");
        auto path=std::filesystem::u8path(filePath);
        if(path.is_relative())path=std::filesystem::u8path(pSettings_->general.EngineRootPath)/path;
        cgl::MapLayout layout;
        const auto status=cgl::IMapLayoutReader::create()->read(path,&layout);
        if(status==cgl::Results::OutOfMemory)throw std::bad_alloc();
        if(status!=cgl::Results::Success)throw std::invalid_argument("Cannot read MAP file: "+filePath);
        return std::make_unique<cgl::py::MapPreview>(std::move(layout),
            *acquireReader<cgl::IGraphicsInfoReader>(infoReaderMap_,cgl::CrossGateVersion::CG_VERSION_Classic),
            *acquirePaletteReader(cgl::EnvironmentPaletteTypes::Daytime));
    }
    py::array_t<uint8_t> acquire_map_page(const cgl::py::MapPreview& view,size_t index) {
        ensureOpen();
        auto pixels=view.page(index,*acquireReader<cgl::IGraphicsDataReader>(dataReaderMap_,cgl::CrossGateVersion::CG_VERSION_Classic));
        auto buffer=std::make_unique<std::vector<uint8_t>>(std::move(pixels.rgba));
        py::capsule owner(buffer.get(),[](void* p){delete static_cast<std::vector<uint8_t>*>(p);});
        auto* data=buffer->data();buffer.release();
        return py::array_t<uint8_t>({py::ssize_t(pixels.height),py::ssize_t(pixels.width),py::ssize_t(4)},
            {py::ssize_t(pixels.width)*4,py::ssize_t(4),py::ssize_t(1)},data,owner);
    }
    std::vector<uint32_t> acquire_available_anime_serial_nums(cgl::CrossGateVersion version) {
        auto* reader=acquireReader<cgl::IAnimeInfoReader>(animeInfoReaderMap_,version);
        std::vector<cgl::AnimeResourceSerialNum> entries;
        if(reader->queryAvailableSerialNums(&entries)!=cgl::Results::Success)
            throw std::runtime_error("Could not enumerate animation serial numbers");
        std::vector<uint32_t> result;result.reserve(entries.size());
        for(const auto& entry:entries) result.push_back(entry.value);
        std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());
        return result;
    }
    py::dict acquire_animation_pixels(cgl::CrossGateVersion version,uint32_t serial,
                                     cgl::DirectionTypes direction,cgl::MotionTypes motion,uint64_t maxBytes) {
        ensureOpen();
        if(!maxBytes || maxBytes>cgl::AnimationDecoder::MaxBytes)
            throw std::invalid_argument("max_bytes must be between 1 and 268435456");
        if(direction>=cgl::DirectionTypes::COUNT || motion>=cgl::MotionTypes::COUNT)
            throw std::invalid_argument("Invalid animation direction or motion");
        const auto readers=animationReaders(version);
        auto pixels=animationDecoder_.build({serial,uint32_t(direction),uint32_t(motion),version},readers,maxBytes);
        auto buffer=std::make_unique<std::vector<uint8_t>>(std::move(pixels.rgba));
        py::capsule owner(buffer.get(),[](void* p){delete static_cast<std::vector<uint8_t>*>(p);});
        auto* data=buffer->data();buffer.release();
        const py::ssize_t width=pixels.width,height=pixels.height;
        py::array_t<uint8_t> rgba({py::ssize_t(pixels.frameCount),height,width,py::ssize_t(4)},
                                {height*width*4,width*4,py::ssize_t(4),py::ssize_t(1)},data,owner);
        return py::dict("width"_a=pixels.width,"height"_a=pixels.height,"frame_count"_a=pixels.frameCount,
                        "duration_ms"_a=pixels.durationMs,"offset_x"_a=pixels.offsetX,"offset_y"_a=pixels.offsetY,"rgba"_a=rgba);
    }
    void ensureOpen() const {
        if (closed_) throw std::runtime_error("Session is closed");
    }
    void trim_cache() noexcept;
    void invalidate_resources() noexcept;
    void close() noexcept { invalidate_resources(); pSettings_.reset(); closed_ = true; }
    void clear_palette_cache() noexcept { if (paletteReader_) paletteReader_->clearCache(); }
    void set_cache_budget(size_t bytes);
    void reset_cache_stats() noexcept;
    py::dict cache_stats() const;

    size_t acquire_graphics_data_count(
        cgl::CrossGateVersion version);

    cgl::py::PaletteData256Holder::Ptr acquire_env_palette(
        cgl::EnvironmentPaletteTypes envPalette);

    std::vector<uint8_t> apply_palette(
        void* pGfxDataHandle,
        void* pPlaetteHandle);

    std::vector<int32_t> acquire_available_serial_nums(
        cgl::CrossGateVersion version);

    cgl::py::GraphicsResourceBundleHolder::Ptr
    acquire_graphics_resource(
        cgl::CrossGateVersion version,
        uint32_t              serial_num);

    cgl::py::AnimeResourceDataHolder::Ptr
    acquire_anime_resource(
        cgl::CrossGateVersion version,
        uint32_t              anime_serial_num);

private:
    std::shared_ptr<uint8_t> animationEpoch_;
    cgl::AnimationReaders animationReaders(cgl::CrossGateVersion version) {
        cgl::AnimationReaders readers{acquireReader<cgl::IAnimeInfoReader>(animeInfoReaderMap_,version),
            acquireReader<cgl::IAnimeDataReader>(animeDataReaderMap_,version),
            acquireReader<cgl::IGraphicsInfoReader>(infoReaderMap_,version),
            acquireReader<cgl::IGraphicsDataReader>(dataReaderMap_,version),
            acquirePaletteReader(cgl::EnvironmentPaletteTypes::Daytime)};
        const auto paletteVersion=cgl::AnimationPaletteVersion(version);
        if(paletteVersion!=version) {
            readers.hiddenPaletteInfo=acquireReader<cgl::IGraphicsInfoReader>(infoReaderMap_,paletteVersion);
            readers.hiddenPaletteData=acquireReader<cgl::IGraphicsDataReader>(dataReaderMap_,paletteVersion);
        }
        return readers;
    }
    cgl::AnimationDecoder animationDecoder_;
    bool closed_ = false;
    size_t cacheBudgetBytes_;

    cgl::SettingsPtr pSettings_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IGraphicsInfoReader::Ptr> infoReaderMap_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IGraphicsDataReader::Ptr> dataReaderMap_;

    cgl::IPaletteReader::Ptr paletteReader_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IAnimeInfoReader::Ptr> animeInfoReaderMap_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IAnimeDataReader::Ptr> animeDataReaderMap_;

    cgl::IPaletteReader* acquirePaletteReader(
        cgl::EnvironmentPaletteTypes envPalette);

    template <typename ReaderInterface, typename MapType>
    ReaderInterface* acquireReader(
        MapType& readerMap,
        const cgl::CrossGateVersion version);
};

}   // namespace


// -----------------------------------------------------------------------------
Session::Session(const std::string& engineRootPath, size_t cacheBudgetBytes)
    : cacheBudgetBytes_(cacheBudgetBytes) {
    if (engineRootPath.empty()) pSettings_ = cgl::LoadSettings();
    else {
        pSettings_ = std::make_unique<cgl::Settings>();
        pSettings_->general.EngineRootPath = std::filesystem::absolute(engineRootPath).lexically_normal().string();
    }
    if (!pSettings_) throw std::runtime_error("Could not load Session settings");
}

void Session::trim_cache() noexcept {
    animationDecoder_.clear();
    for (auto& [version, reader] : dataReaderMap_) reader->trimCache();
    for (auto& [version, reader] : animeDataReaderMap_) reader->trimCache();
    clear_palette_cache();
}
void Session::invalidate_resources() noexcept {
    animationEpoch_.reset();
    animationDecoder_.clear();
    auto release = [](auto& readers) { std::decay_t<decltype(readers)>().swap(readers); };
    release(infoReaderMap_); release(dataReaderMap_);
    release(animeInfoReaderMap_); release(animeDataReaderMap_);
    paletteReader_.reset();
}
void Session::set_cache_budget(size_t bytes) {
    ensureOpen();
    cacheBudgetBytes_ = bytes;
    for (auto& [version, reader] : dataReaderMap_) reader->setCacheBudget(bytes);
    for (auto& [version, reader] : animeDataReaderMap_) reader->setCacheBudget(bytes);
}
void Session::reset_cache_stats() noexcept {
    for (auto& [version, reader] : dataReaderMap_) reader->resetCacheStatistics();
    for (auto& [version, reader] : animeDataReaderMap_) reader->resetCacheStatistics();
}
py::dict Session::cache_stats() const {
    py::list readers;
    uint64_t resident = 0, scratch = 0, calls = 0, bytes = 0;
    py::object budget = py::int_(0);
    auto append = [&](const auto& map, const char* kind) {
        for (unsigned i = 0; i < unsigned(cgl::CrossGateVersion::COUNT); ++i) {
            auto version = cgl::CrossGateVersion(i);
            auto found = map.find(version);
            if (found == map.end()) continue;
            const auto s = found->second->cacheStatistics();
            std::ostringstream name; name << version;
            readers.append(py::dict(
                "kind"_a=kind, "version"_a=name.str(), "budget_bytes"_a=s.budgetBytes,
                "resident_bytes"_a=s.residentBytes, "peak_resident_bytes"_a=s.peakResidentBytes,
                "cached_blocks"_a=s.cachedBlocks, "scratch_bytes"_a=s.scratchBytes,
                "hits"_a=s.hits, "misses"_a=s.misses, "evictions"_a=s.evictions,
                "file_read_calls"_a=s.fileReadCalls, "file_read_bytes"_a=s.fileReadBytes,
                "failed_reads"_a=s.failedReads, "bypass_reads"_a=s.bypassReads));
            resident += s.residentBytes; scratch += s.scratchBytes; budget = budget + py::int_(s.budgetBytes);
            calls += s.fileReadCalls; bytes += s.fileReadBytes;
        }
    };
    append(dataReaderMap_, "graphics"); append(animeDataReaderMap_, "anime");
    return py::dict("closed"_a=closed_, "cache_budget_per_reader_bytes"_a=cacheBudgetBytes_,
        "readers"_a=readers, "graphics_index_readers"_a=infoReaderMap_.size(),
        "anime_index_readers"_a=animeInfoReaderMap_.size(), "palette_reader_active"_a=bool(paletteReader_),
        "totals"_a=py::dict("resident_bytes"_a=resident, "scratch_bytes"_a=scratch,
            "budget_bytes"_a=budget, "file_read_calls"_a=calls, "file_read_bytes"_a=bytes));
}

// -----------------------------------------------------------------------------
template <typename ReaderInterface, typename MapType>
ReaderInterface* Session::acquireReader(
    MapType& readerMap,
    const cgl::CrossGateVersion version
) {
    ensureOpen();
    if (version >= cgl::CrossGateVersion::COUNT) throw std::invalid_argument("Invalid resource version");
    if (auto found = readerMap.find(version); found != readerMap.end()) return found->second.get();
    typename ReaderInterface::CreateInfo info{.pSettings = pSettings_.get(), .version = version};
    if constexpr (std::is_same_v<ReaderInterface, cgl::IGraphicsDataReader> ||
                  std::is_same_v<ReaderInterface, cgl::IAnimeDataReader>) {
        info.cacheBudgetBytes = cacheBudgetBytes_;
    }
    auto reader = ReaderInterface::create(info);
    if (!reader || reader->load() != cgl::Results::Success) {
        std::stringstream message;
        message << "Failed to load " << type_name<ReaderInterface>() << " for version: " << version;
        throw std::invalid_argument(message.str());
    }
    // Publish only successfully loaded readers; failures can retry on the next call.
    auto* result = reader.get();
    readerMap.emplace(version, std::move(reader));
    return result;
}

// -----------------------------------------------------------------------------
cgl::IPaletteReader* Session::acquirePaletteReader(
    cgl::EnvironmentPaletteTypes envPalette
) {
    ensureOpen();
    if (!paletteReader_) {
        paletteReader_ = cgl::IPaletteReader::create({
            .pSettings = pSettings_.get(),
        });
        if (!paletteReader_) {
            throw std::invalid_argument("Failed to create the reader");
        }
    }
    return paletteReader_.get();
}

// -----------------------------------------------------------------------------
size_t Session::acquire_graphics_data_count(
    cgl::CrossGateVersion version
) {
    auto reader = acquireReader<cgl::IGraphicsInfoReader>(
                    infoReaderMap_, version);
    return reader->infoCount();
}

// -----------------------------------------------------------------------------
cgl::py::PaletteData256Holder::Ptr Session::acquire_env_palette(
    cgl::EnvironmentPaletteTypes envPalette
) {
    auto infoReader = acquirePaletteReader(envPalette);

    auto data = std::make_shared<cgl::py::PaletteData256Holder>();
    if (infoReader->read(envPalette, &data->data) != cgl::Results::Success) {
        LOGE("Failed to query palette " << envPalette);
        return nullptr;
    }
    return data;
}

// -----------------------------------------------------------------------------
std::vector<int32_t> Session::acquire_available_serial_nums(
    cgl::CrossGateVersion version
) {
    auto reader = acquireReader<cgl::IGraphicsInfoReader>(
                    infoReaderMap_, version);

    std::vector<int32_t> ret;
    if (reader->queryAvailableSerialNums(&ret) != cgl::Results::Success) {
        throw std::invalid_argument("Failed to query available Serial Numbers");
    }
    return ret;
}

// -----------------------------------------------------------------------------
std::vector<uint8_t> Session::apply_palette(
    void* pGfxDataHandle,
    void* pPletteHandle
) {
    auto pHolder = static_cast<cgl::py::GraphicsResourceBundleHolder *>(pGfxDataHandle);
    auto pPaletteHolder = static_cast<cgl::py::PaletteData256Holder *>(pPletteHandle);
    if (!pHolder || !pPaletteHolder) throw std::invalid_argument("Graphics and palette holders are required");
    auto pGfxResData = &pHolder->data;
    if (!pGfxResData->pData || pGfxResData->width <= 0 || pGfxResData->height <= 0)
        throw std::invalid_argument("Graphics holder has no pixels");
    std::vector<uint8_t> img;
    size_t count = size_t(pGfxResData->width) * size_t(pGfxResData->height);
    img.resize(count * 4);
    uint8_t* pData = pGfxResData->pData.get();
    uint8_t* pImg = img.data();
    uint8_t* pEnd = img.data() + count * 4;

    // const cgl::PaletteData256* pPalette = &pPaletteHolder->data;
    auto pPalette = reinterpret_cast<uint8_t*>(pPaletteHolder->data.data());
    if (pGfxResData->pPaletteData != nullptr) {
        pPalette = pGfxResData->pPaletteData;
    }

    while (pImg < pEnd) {
        uint32_t paletteDataIdx = *(pData);
        uint8_t* pBGR = &pPalette[paletteDataIdx * 3];
        pImg[0] = pBGR[0];
        pImg[1] = pBGR[1];
        pImg[2] = pBGR[2];
        pImg[3] = (paletteDataIdx == 0)? 0 : 255;
        pImg+=4;
        pData++;
    }

    return img;
}

// -----------------------------------------------------------------------------
cgl::py::AnimeResourceDataHolder::Ptr Session::acquire_anime_resource(
    cgl::CrossGateVersion version,
    uint32_t              serial_num
) {
    auto infoReader = acquireReader<cgl::IAnimeInfoReader>(
                        animeInfoReaderMap_, version);
    auto dataReader = acquireReader<cgl::IAnimeDataReader>(
                        animeDataReaderMap_, version);

    if (!infoReader || !dataReader) {
        LOGE("Failed to acquire anime readers for version: " << version);
        return nullptr;
    }

    cgl::AnimeResourceInfo info;
    cgl::AnimeResourceSerialNum sn {
        .version = version,
        .value   = serial_num
    };

    // query info first
    if (infoReader->query(sn, &info) != cgl::Results::Success) {
        LOGE("Failed to query anime info via S/N: " << sn);
        return nullptr;
    }

    // query anime data to the holder
    auto holder = std::make_shared<cgl::py::AnimeResourceDataHolder>();
    if (dataReader->query(info, &holder->animeData) != cgl::Results::Success) {
        LOGE("Failed to query anime data via S/N: " << sn);
        return nullptr;
    }

    return holder;
}

// -----------------------------------------------------------------------------
cgl::py::GraphicsResourceBundleHolder::Ptr Session::acquire_graphics_resource(
    cgl::CrossGateVersion version,
    uint32_t              serial_num
) {
    auto infoReader = acquireReader<cgl::IGraphicsInfoReader>(
                        infoReaderMap_, version);
    auto dataReader = acquireReader<cgl::IGraphicsDataReader>(
                        dataReaderMap_, version);

    if (!infoReader || !dataReader) {
        LOGE("Failed to acquire graphics readers for version: " << version);
        return nullptr;
    }

    auto holder = std::make_shared<cgl::py::GraphicsResourceBundleHolder>();

    // query info first
    cgl::GraphicsResourceSerialNum sn {
        .version = version,
        .value = serial_num
    };

    if (infoReader->query(sn, &holder->info) != cgl::Results::Success) {
        LOGE("Failed to query graphics info via S/N: " << sn);
        return nullptr;
    }

    if (dataReader->query(holder->info, &holder->data) != cgl::Results::Success) {
        LOGE("Failed to query graphics data via S/N: " << sn);
        return nullptr;
    }

    return holder;
}

// -----------------------------------------------------------------------------
// Define pybind
// -----------------------------------------------------------------------------
PYBIND11_MODULE(cgl, m) {

    cgl::SetLogLevel(cgl::LogLevel::Trace);
    py::class_<AnimationPlan>(m,"AnimationPlan")
        .def_property_readonly("metadata",&AnimationPlan::metadata);

    py::class_<cgl::py::MapPreview>(m,"MapPreview")
        .def_property_readonly("width",&cgl::py::MapPreview::width)
        .def_property_readonly("height",&cgl::py::MapPreview::height)
        .def_property_readonly("page_count",&cgl::py::MapPreview::pageCount)
        .def("pan",&cgl::py::MapPreview::pan,py::arg("x"),py::arg("y"),py::arg("dx"),py::arg("dy"),py::arg("scale"))
        .def("draw_list",&cgl::py::MapPreview::drawList,py::arg("x"),py::arg("y"),py::arg("width"),py::arg("height"),
             py::arg("scale")=1.0,py::arg("ground")=true,py::arg("objects")=true);

    py::class_<cgl::py::MapSnapshot>(m, "MapSnapshot")
        .def_property_readonly("width", &cgl::py::MapSnapshot::width)
        .def_property_readonly("height", &cgl::py::MapSnapshot::height)
        .def("cell", &cgl::py::MapSnapshot::cell, py::arg("x"), py::arg("y"))
        .def("region", &cgl::py::MapSnapshot::region, py::arg("x"), py::arg("y"),
             py::arg("width"), py::arg("height"));

    // -------------------------------------------------------------------------
    // Session
    // -------------------------------------------------------------------------
    py::class_<Session>(m, "Session")
        .def(py::init<const std::string&, size_t>(), py::arg("engine_root_path") = "",
             py::arg("cache_budget_bytes") = 4 * 1024 * 1024)
        .def("acquire_available_anime_serial_nums", &Session::acquire_available_anime_serial_nums,py::arg("version"))
        .def("plan_animation",&Session::plan_animation,py::arg("version"),py::arg("serial"),py::arg("direction"),py::arg("motion"))
        .def("decode_animation_frame",&Session::decode_animation_frame,py::arg("plan"),py::arg("index"),py::arg("max_bytes")=4ull*1024*1024)
        .def("load_map", &Session::load_map, py::arg("file_path"))
        .def("load_map_preview",&Session::load_map_preview,py::arg("file_path"))
        .def("acquire_map_page",&Session::acquire_map_page,py::arg("preview"),py::arg("index"))
        .def("acquire_animation_pixels", &Session::acquire_animation_pixels,py::arg("version"),py::arg("serial"),
             py::arg("direction"),py::arg("motion"),py::arg("max_bytes")=64ull*1024*1024)
        .def("cache_stats", &Session::cache_stats)
        .def("trim_cache", &Session::trim_cache)
        .def("clear_palette_cache", &Session::clear_palette_cache)
        .def("invalidate_resources", &Session::invalidate_resources)
        .def("set_cache_budget", &Session::set_cache_budget, py::arg("bytes"))
        .def("reset_cache_stats", &Session::reset_cache_stats)
        .def("close", &Session::close)
        .def("__enter__", [](Session& self) -> Session& { self.ensureOpen(); return self; },
             py::return_value_policy::reference_internal)
        .def("__exit__", [](Session& self, py::object, py::object, py::object) { self.close(); return false; })

        .def(
            "acquire_graphics_data_count",
            &Session::acquire_graphics_data_count)

        .def(
            "acquire_graphics_resource",
            [](Session&              self,
               cgl::CrossGateVersion version,
               uint32_t              serial_num
            ) -> py::object {
                auto ptr = self.acquire_graphics_resource(version, serial_num);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })

        .def(
            "acquire_env_palette",
            [](Session&                     self,
               cgl::EnvironmentPaletteTypes envPalette
            ) -> py::object {
                auto ptr = self.acquire_env_palette(envPalette);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })

        .def(
            "acquire_available_serial_nums",
            [](Session& self,
               cgl::CrossGateVersion version) {
                return self.acquire_available_serial_nums(version);
            })

        .def(
            "apply_palette",
            [](Session&                                   self,
               cgl::py::GraphicsResourceBundleHolder::Ptr gfx_data,
               cgl::py::PaletteData256Holder::Ptr         palette_data
            ) -> py::object {
                auto img = self.apply_palette(gfx_data.get(), palette_data.get());
                auto buffer = std::make_unique<std::vector<uint8_t>>(std::move(img));
                uint8_t* data_ptr = buffer->data();
                py::capsule free_when_done(buffer.get(), [](void* f) {
                    delete static_cast<std::vector<uint8_t>*>(f);
                });

                buffer.release(); // The capsule owns it, including if array construction throws.
                return py::array_t<uint8_t>(
                    { py::ssize_t(gfx_data->data.height), py::ssize_t(gfx_data->data.width), py::ssize_t(4) },
                    { py::ssize_t(gfx_data->data.width) * 4, py::ssize_t(4), py::ssize_t(1) },
                    data_ptr,
                    free_when_done
                );
            })

        .def(
            "acquire_anime_resource",
            [](Session&              self,
               cgl::CrossGateVersion version,
               size_t                serial_num
            ) -> py::object {
                auto ptr = self.acquire_anime_resource(version, serial_num);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })
        ;

    // -------------------------------------------------------------------------
    // Define cgl::CrossGateVersion
    // -------------------------------------------------------------------------
    m.def("CrossGateVersionList", &GetTypes_CrossGateVersion);
    py::enum_<cgl::CrossGateVersion>(m, "CrossGateVersion", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::CrossGateVersion::name)
            CGL_VERSION_TYPES_VALID_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::EnvironmentPaletteTypes
    // -------------------------------------------------------------------------
    m.def("EnvironmentPaletteTypesList", &GetTypes_EnvironmentPaletteTypes);
    py::enum_<cgl::EnvironmentPaletteTypes>(m, "EnvironmentPaletteTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::EnvironmentPaletteTypes::name)
            CGL_ENVIRONMENT_PALETTE_TYPES_ENUM_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::DirectionTypes
    // -------------------------------------------------------------------------
    py::enum_<cgl::DirectionTypes>(m, "DirectionTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::DirectionTypes::name)
            CGL_DIRECTION_TYPES_ENUM_FULL_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::MotionTypes
    // -------------------------------------------------------------------------
    py::enum_<cgl::MotionTypes>(m, "MotionTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::MotionTypes::name)
            CGL_MOTION_TYPES_ENUM_FULL_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::GraphicsResourceBundleHolder
    // -------------------------------------------------------------------------
    py::class_<cgl::py::GraphicsResourceBundleHolder,
               cgl::py::GraphicsResourceBundleHolder::Ptr>(m, "GraphicsResourceBundleHolder")
        .def("offset",
             &cgl::py::GraphicsResourceBundleHolder::offset,
             "Return offset as (offsetX, offsetY) in current graphics resource");
        // .def_readonly("version", &cgl::GraphicsResourceBundleHolder::version)
        // .def_readonly("width", &cgl::GraphicsResourceBundleHolder::width)
        // .def_readonly("height", &cgl::GraphicsResourceBundleHolder::height)
        ;

    // -------------------------------------------------------------------------
    // Define holders
    // -------------------------------------------------------------------------
    py::class_<cgl::py::PaletteData256Holder, cgl::py::PaletteData256Holder::Ptr>(m, "PaletteData256Holder")
        .def(py::init<>());

    py::class_<cgl::py::AnimeResourceDataHolder, cgl::py::AnimeResourceDataHolder::Ptr>(m, "AnimeResourceDataHolder")
        .def(py::init<>())
        .def("keys", &cgl::py::AnimeResourceDataHolder::keys)
        .def("acquire", &cgl::py::AnimeResourceDataHolder::acquire);

    py::class_<cgl::AnimeMotionDesc>(m, "AnimeMotionDesc")
        .def_readonly("version", &cgl::AnimeMotionDesc::version)
        .def_readonly("direction", &cgl::AnimeMotionDesc::direction)
        .def_readonly("motion", &cgl::AnimeMotionDesc::motion)
        .def_readonly("duration", &cgl::AnimeMotionDesc::duration)
        .def_readonly("extended_flags", &cgl::AnimeMotionDesc::extendedFlags)
        .def_readonly("motionGraphicsSerialNums", &cgl::AnimeMotionDesc::motionGraphicsSerialNums)
        .def("__repr__", [](const cgl::AnimeMotionDesc& desc) {
            return "<AnimeMotionDesc: " + std::to_string(static_cast<int>(desc.direction)) +
                   ", " + std::to_string(static_cast<int>(desc.motion)) + ">";
        });
}
